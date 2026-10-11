# SSAS - Simple Smart Automotive Software
# Copyright (C) 2024 Parai Wang <parai@foxmail.com>
import argparse
import collections
import os, sys
import traceback
import json
import re
from elftools.elf.elffile import ELFFile

from elftools.common.exceptions import DWARFError, ELFError
from elftools.common.utils import bytes2str
from elftools.dwarf.locationlists import (
    LocationParser,
    LocationEntry,
    LocationExpr,
    LocationViewPair,
    BaseAddressEntry as LocBaseAddressEntry,
)
from elftools.dwarf.ranges import (
    RangeEntry,
)  # ranges.BaseAddressEntry collides with the one above
import elftools.dwarf.ranges
from elftools.dwarf.enums import *
from elftools.dwarf.dwarf_expr import DWARFExprParser, DWARFExprOp
from elftools.dwarf.datatype_cpp import DIE_name, describe_cpp_datatype
from elftools.dwarf.descriptions import describe_reg_name


def _get_cu_base(cu):
    top_die = cu.get_top_DIE()
    attr = top_die.attributes
    if "DW_AT_low_pc" in attr:
        return attr["DW_AT_low_pc"].value
    elif "DW_AT_entry_pc" in attr:
        return attr["DW_AT_entry_pc"].value
    else:
        raise ValueError("Can't find the base IP (low_pc) for a CU")


def _addr_str_length(die):
    return die.cu.header.address_size * 2


def _DIE_name(die):
    if "DW_AT_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_name"].value)
    elif "DW_AT_linkage_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_linkage_name"].value)
    else:
        raise DWARFError()


def _DIE_linkage_name(die):
    if "DW_AT_linkage_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_linkage_name"].value)
    elif "DW_AT_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_name"].value)
    else:
        raise DWARFError()


def _safe_DIE_name(die, default=None):
    if "DW_AT_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_name"].value)
    elif "DW_AT_linkage_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_linkage_name"].value)
    else:
        return default


def _safe_DIE_linkage_name(die, default=None):
    if "DW_AT_linkage_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_linkage_name"].value)
    elif "DW_AT_name" in die.attributes:
        return bytes2str(die.attributes["DW_AT_name"].value)
    else:
        return default


def _desc_ref(attr, die, extra=""):
    if extra:
        extra = ' "%s"' % extra
    return "cu + 0x%04x => {0x%08x}%s" % (
        attr.raw_value,
        die.cu.cu_offset + attr.raw_value,
        extra,
    )


def _desc_data(attr, die):
    """Hex with length driven by form"""
    len = int(attr.form[12:]) * 2
    return "0x%0*x" % (
        len,
        attr.value,
    )


def _desc_strx(attr, die):
    return 'indexed (%08x) string = "%s"' % (
        attr.raw_value,
        bytes2str(attr.value).replace("\\", "\\\\"),
    )


FORM_DESCRIPTIONS = dict(
    DW_FORM_string=lambda attr, die: '"%s"' % (bytes2str(attr.value),),
    DW_FORM_strp=lambda attr, die: ' .debug_str[0x%08x] = "%s"'
    % (attr.raw_value, bytes2str(attr.value).replace("\\", "\\\\")),
    DW_FORM_strx1=_desc_strx,
    DW_FORM_strx2=_desc_strx,
    DW_FORM_strx3=_desc_strx,
    DW_FORM_strx4=_desc_strx,
    DW_FORM_line_strp=lambda attr, die: '.debug_line_str[0x%08x] = "%s"'
    % (attr.raw_value, bytes2str(attr.value).replace("\\", "\\\\")),
    DW_FORM_flag_present=lambda attr, die: "true",
    DW_FORM_flag=lambda attr, die: "0x%02x" % int(attr.value),
    DW_FORM_addr=lambda attr, die: "0x%0*x" % (_addr_str_length(die), attr.value),
    DW_FORM_addrx=lambda attr, die: "indexed (%08x) address = 0x%0*x"
    % (attr.raw_value, _addr_str_length(die), attr.value),
    DW_FORM_data1=_desc_data,
    DW_FORM_data2=_desc_data,
    DW_FORM_data4=_desc_data,
    DW_FORM_data8=_desc_data,
    DW_FORM_block1=lambda attr, die: "<0x%02x> %s " % (len(attr.value), " ".join("%02x" % b for b in attr.value)),
    DW_FORM_block2=lambda attr, die: "<0x%04x> %s " % (len(attr.value), " ".join("%02x" % b for b in attr.value)),
    DW_FORM_block4=lambda attr, die: "<0x%08x> %s " % (len(attr.value), " ".join("%02x" % b for b in attr.value)),
    DW_FORM_ref=_desc_ref,
    DW_FORM_ref1=_desc_ref,
    DW_FORM_ref2=_desc_ref,
    DW_FORM_ref4=_desc_ref,
    DW_FORM_ref8=_desc_ref,
    DW_FORM_sec_offset=lambda attr, die: "0x%08x" % (attr.value,),
    DW_FORM_exprloc=lambda attr, die: _desc_expression(attr.value, die),
)


def _desc_enum(attr, enum):
    """For attributes like DW_AT_language, physically
    int, logically an enum
    """
    return next((k for (k, v) in enum.items() if v == attr.value), str(attr.value))


def _cu_comp_dir(cu):
    return bytes2str(cu.get_top_DIE().attributes["DW_AT_comp_dir"].value)


def _desc_decl_file(attr, die):
    # Filename/dirname arrays are 0 based in DWARFv5
    cu = die.cu
    if not hasattr(cu, "_lineprogram"):
        cu._lineprogram = die.dwarfinfo.line_program_for_CU(cu)
    ver5 = cu._lineprogram.header.version >= 5
    file_index = attr.value if ver5 else attr.value - 1
    if cu._lineprogram and file_index >= 0 and file_index < len(cu._lineprogram.header.file_entry):
        file_entry = cu._lineprogram.header.file_entry[file_index]
        dir_index = file_entry.dir_index if ver5 else file_entry.dir_index - 1
        includes = cu._lineprogram.header.include_directory
        if dir_index >= 0:
            dir = bytes2str(includes[dir_index])
            if dir.startswith("."):
                dir = os.path.join(_cu_comp_dir(cu), dir)
        else:
            dir = _cu_comp_dir(cu)
        file_name = bytes2str(file_entry.name)
    else:
        raise DWARFError("Invalid source filename entry index in a decl_file attribute")
    return '"%s"' % (os.path.join(dir, file_name),)


def _desc_ranges(attr, die):
    di = die.cu.dwarfinfo
    if not hasattr(di, "_rnglists"):
        di._rangelists = di.range_lists()
    rangelist = di._rangelists.get_range_list_at_offset(attr.value, die.cu)
    base_ip = _get_cu_base(die.cu)
    lines = []
    addr_str_len = die.cu.header.address_size * 2
    for entry in rangelist:
        if isinstance(entry, RangeEntry):
            lines.append(
                "                 [0x%0*x, 0x%0*x)"
                % (
                    addr_str_len,
                    (0 if entry.is_absolute else base_ip) + entry.begin_offset,
                    addr_str_len,
                    (0 if entry.is_absolute else base_ip) + entry.end_offset,
                )
            )
        elif isinstance(entry, elftools.dwarf.ranges.BaseAddressEntry):
            base_ip = entry.base_address
        else:
            raise NotImplementedError("Unknown object in a range list")
    prefix = "indexed (0x%x) rangelist = " % attr.raw_value if attr.form == "DW_FORM_rnglistx" else ""
    return ("%s0x%08x\n" % (prefix, attr.value)) + "\n".join(lines)


def _desc_locations(attr, die):
    cu = die.cu
    di = cu.dwarfinfo
    if not hasattr(di, "_loclists"):
        di._loclists = di.location_lists()
    if not hasattr(di, "_locparser"):
        di._locparser = LocationParser(di._loclists)
    loclist = di._locparser.parse_from_attribute(attr, cu.header.version, die)
    if isinstance(loclist, LocationExpr):
        return _desc_expression(loclist.loc_expr, die)
    else:
        base_ip = _get_cu_base(cu)
        lines = []
        addr_str_len = die.cu.header.address_size * 2
        for entry in loclist:
            if isinstance(entry, LocationEntry):
                lines.append(
                    "                 [0x%0*x, 0x%0*x): %s"
                    % (
                        addr_str_len,
                        (0 if entry.is_absolute else base_ip) + entry.begin_offset,
                        addr_str_len,
                        (0 if entry.is_absolute else base_ip) + entry.end_offset,
                        _desc_expression(entry.loc_expr, die),
                    )
                )
            elif isinstance(entry, LocBaseAddressEntry):
                base_ip = entry.base_address
            else:
                raise NotImplementedError("Unknown object in a location list")
        prefix = "indexed (0x%x) loclist = " % attr.raw_value if attr.form == "DW_FORM_loclistx" else ""
        return ("%s0x%08x:\n" % (prefix, attr.value)) + "\n".join(lines)


# By default, numeric arguments are spelled in hex with a leading 0x
def _desc_operationarg(s, cu):
    if isinstance(s, str):
        return s
    elif isinstance(s, int):
        return hex(s)
    elif isinstance(s, list):  # Could be a blob (list of ints), could be a subexpression
        if len(s) > 0 and isinstance(s[0], DWARFExprOp):  # Subexpression
            return "(" + "; ".join(_desc_operation(op.op, op.op_name, op.args, cu) for op in s) + ")"
        else:
            return " ".join((hex(len(s)),) + tuple("0x%02x" % b for b in s))


def _arch(cu):
    return cu.dwarfinfo.config.machine_arch


def _desc_reg(reg_no, cu):
    return describe_reg_name(reg_no, _arch(cu), True).upper()


def _desc_operation(op, op_name, args, cu):
    # Not sure about regx(regno) and bregx(regno, offset)
    if 0x50 <= op <= 0x6F:  # reg0...reg31 - decode reg name
        return op_name + " " + _desc_reg(op - 0x50, cu)
    elif 0x70 <= op <= 0x8F:  # breg0...breg31(offset) - also decode reg name
        return "%s %s%+d" % (op_name, _desc_reg(op - 0x70, cu), args[0])
    elif op_name in (
        "DW_OP_fbreg",
        "DW_OP_bra",
        "DW_OP_skip",
        "DW_OP_consts",
    ):  # Argument is decimal with a leading sign
        return op_name + " " + "%+d" % (args[0])
    elif op_name in (
        "DW_OP_const1s",
        "DW_OP_const2s",
    ):  # Argument is decimal without a leading sign
        return op_name + " " + "%d" % (args[0])
    elif op_name in (
        "DW_OP_entry_value",
        "DW_OP_GNU_entry_value",
    ):  # No space between opcode and args
        return op_name + _desc_operationarg(args[0], cu)
    elif op_name == "DW_OP_regval_type":  # Arg is a DIE pointer
        return '%s %s (0x%08x -> 0x%08x) "%s"' % (
            op_name,
            _desc_reg(args[0], cu),
            args[1],
            args[1] + cu.cu_offset,
            _DIE_name(cu._get_cached_DIE(args[1] + cu.cu_offset)),
        )
    elif op_name == "DW_OP_convert":  # Arg is a DIE pointer
        return '%s (0x%08x -> 0x%08x) "%s"' % (
            op_name,
            args[0],
            args[0] + cu.cu_offset,
            _DIE_name(cu._get_cached_DIE(args[0] + cu.cu_offset)),
        )
    elif args:
        return op_name + " " + ", ".join(_desc_operationarg(s, cu) for s in args)
    else:
        return op_name


# TODO: remove this once dwarfdump catches up
UNSUPPORTED_OPS = (
    "DW_OP_implicit_pointer",
    "DW_OP_deref_type",
    "DW_OP_GNU_parameter_ref",
    "DW_OP_GNU_deref_type",
    "DW_OP_GNU_implicit_pointer",
    "DW_OP_GNU_convert",
    "DW_OP_GNU_regval_type",
)


def _desc_expression(expr, die):
    cu = die.cu
    if not hasattr(cu, "_exprparser"):
        cu._exprparser = DWARFExprParser(cu.structs)

    parsed = cu._exprparser.parse_expr(expr)
    # TODO: remove this once dwarfdump catches up
    first_unsupported = next((i for (i, op) in enumerate(parsed) if op.op_name in UNSUPPORTED_OPS), None)
    if first_unsupported is None:
        lines = [_desc_operation(op.op, op.op_name, op.args, cu) for op in parsed]
    else:
        lines = [_desc_operation(op.op, op.op_name, op.args, cu) for op in parsed[0:first_unsupported]]
        start_of_unparsed = parsed[first_unsupported].offset
        lines.append("<decoding error> " + " ".join("%02x" % b for b in expr[start_of_unparsed:]))
    return ", ".join(lines)


def _desc_datatype(attr, die):
    """Oy vey"""
    return _desc_ref(attr, die, describe_cpp_datatype(die))


def _get_origin_name(die):
    func_die = die.get_DIE_from_attribute("DW_AT_abstract_origin")
    name = _safe_DIE_linkage_name(func_die, "")
    if not name:
        if "DW_AT_specification" in func_die.attributes:
            name = _DIE_linkage_name(func_die.get_DIE_from_attribute("DW_AT_specification"))
        elif "DW_AT_abstract_origin" in func_die.attributes:
            return _get_origin_name(func_die)
    return name


def _desc_origin(attr, die):
    return _desc_ref(attr, die, _get_origin_name(die))


def _desc_spec(attr, die):
    return _desc_ref(attr, die, _DIE_linkage_name(die.get_DIE_from_attribute("DW_AT_specification")))


def _desc_value(attr, die):
    return str(attr.value)


ATTR_DESCRIPTIONS = dict(
    DW_AT_language=lambda attr, die: _desc_enum(attr, ENUM_DW_LANG),
    DW_AT_encoding=lambda attr, die: _desc_enum(attr, ENUM_DW_ATE),
    DW_AT_accessibility=lambda attr, die: _desc_enum(attr, ENUM_DW_ACCESS),
    DW_AT_inline=lambda attr, die: _desc_enum(attr, ENUM_DW_INL),
    DW_AT_calling_convention=lambda attr, die: _desc_enum(attr, ENUM_DW_CC),
    DW_AT_decl_file=_desc_decl_file,
    DW_AT_decl_line=_desc_value,
    DW_AT_ranges=_desc_ranges,
    DW_AT_location=_desc_locations,
    DW_AT_data_member_location=lambda attr, die: (
        _desc_data(attr, die) if attr.form.startswith("DW_FORM_data") else _desc_locations(attr, die)
    ),
    DW_AT_frame_base=_desc_locations,
    DW_AT_type=_desc_datatype,
    DW_AT_call_line=_desc_value,
    DW_AT_call_file=_desc_decl_file,
    DW_AT_abstract_origin=_desc_origin,
    DW_AT_specification=_desc_spec,
    DW_AT_call_site_value=lambda attr, die: (
        _desc_expression(attr.value, die) if attr.form.startswith("DW_FORM_block") else _desc_locations(attr, die)
    ),
    DW_AT_GNU_call_site_value=lambda attr, die: (
        _desc_expression(attr.value, die) if attr.form.startswith("DW_FORM_block") else _desc_locations(attr, die)
    ),
)


COMPOSITE_TAGS = {
    "DW_TAG_structure_type": "structure",
    "DW_TAG_union_type": "union",
    "DW_TAG_enumeration_type": "enumeration",
}

# a C qualifier is a type node of its own: `volatile` is not part of the name of the
# type it qualifies, it is a DIE pointing at it
QUALIFIER_TAGS = {
    "DW_TAG_const_type": "const",
    "DW_TAG_volatile_type": "volatile",
    "DW_TAG_restrict_type": "restrict",
}

# an unnamed composite can only be reached through the DIE it points at
ANON_PREFIX = "$anon_0x"


class ElfParser:
    def __init__(
        self,
        elfFilePath,
    ):
        self.elfFilePath = elfFilePath
        with open(elfFilePath, "rb") as file:
            self.elffile = ELFFile(file)
            self._dwarfinfo = self.elffile.get_dwarf_info()

    def describe_attr_value(self, die, attr):
        """This describes the attribute value in the way that's compatible
        with llvm_dwarfdump. Somewhat duplicates the work of describe_attr_value() in descriptions
        """
        if attr.name in ATTR_DESCRIPTIONS:
            return ATTR_DESCRIPTIONS[attr.name](attr, die)
        elif attr.form in FORM_DESCRIPTIONS:
            return FORM_DESCRIPTIONS[attr.form](attr, die)
        else:
            return str(attr.value)

    def debugDIE(self, die):
        print(
            "0x%08x: %s [%d] %s %s"
            % (
                die.offset,
                die.tag,
                die.abbrev_code,
                "*" if die.has_children else "",
                ("(0x%08x)" % die.get_parent().offset if die.get_parent() is not None else ""),
            )
        )
        for attr_name in die.attributes:
            attr = die.attributes[attr_name]
            print(
                "              %s [%s]	(%s)"
                % (
                    attr_name,
                    attr.form,
                    self.describe_attr_value(die, attr),
                )
            )

    def getCpuEndian(self):
        if self.elffile.little_endian:
            return "LITTLE"
        else:
            return "BIG"

    def scanComposites(self):
        """Shape, byte size and location of every type DIE, keyed by its offset."""
        if hasattr(self, "typeShape"):
            # already scanned: the result is complete, and re-running it would wipe the
            # `$anon_` shapes added while the variables were parsed
            return
        self.typeSize = {}
        self.typeLoc = {}
        self.typeShape = {}
        for cu in self._dwarfinfo.iter_CUs():
            for die in cu.iter_DIEs():
                if die.is_null():
                    continue
                id_v = "0x%08x" % die.offset
                # any DIE can carry the size: base types and enums as well, and a
                # typedef is sized by following its DW_AT_type down to that DIE
                size = die.attributes.get("DW_AT_byte_size")
                if size is not None:
                    self.typeSize.setdefault(id_v, size.value)
                shape = COMPOSITE_TAGS.get(die.tag)
                if shape is None:
                    continue
                self.typeShape.setdefault(id_v, shape)
                self.typeLoc.setdefault(id_v, self.die_location(die))

    def die_byte_size(self, die, depth=0):
        """Byte size of a type DIE, following qualifiers, typedefs and arrays."""
        if die is None or depth > 32:
            return None
        size = die.attributes.get("DW_AT_byte_size")
        if size is not None:
            return size.value
        if "DW_TAG_pointer_type" == die.tag:
            return self.elffile.elfclass // 8
        t = die.attributes.get("DW_AT_type")
        if t is None:
            return None
        try:
            target = die.get_DIE_from_attribute("DW_AT_type")
        except Exception:
            return None
        if "DW_TAG_array_type" == die.tag:
            count = 0
            for child in die.iter_children():
                if "DW_TAG_subrange_type" != child.tag:
                    continue
                upper = child.attributes.get("DW_AT_upper_bound")
                cnt = child.attributes.get("DW_AT_count")
                if upper is not None:
                    count += upper.value + 1
                elif cnt is not None:
                    count += cnt.value
            elem = self.die_byte_size(target, depth + 1)
            return None if (elem is None or 0 == count) else elem * count
        return self.die_byte_size(target, depth + 1)

    def parseMembers(self):
        self.scanComposites()
        members = {}
        for cu in self._dwarfinfo.iter_CUs():
            if cu.header.version >= 5:
                unit_type_str = " unit_type = %s," % cu.header.unit_type
            else:
                unit_type_str = ""
            parent = cu.get_top_DIE()
            for die in cu.iter_DIEs():
                if die.get_parent() == parent:
                    parent = die
                if not die.is_null():
                    if die.tag == "DW_TAG_member" and "DW_AT_name" in die.attributes:
                        parent_die = die.get_parent()
                        is_union = parent_die is not None and "DW_TAG_union_type" == parent_die.tag
                        # a union member has no location, they all start at offset 0
                        if "DW_AT_data_member_location" in die.attributes or is_union:
                            # self.debugDIE(die)
                            name_v = _safe_DIE_name(die)
                            type = die.attributes.get("DW_AT_type")
                            type_v = self.die_type_name(die) or "void"
                            if type_v in ("structure", "union", "enumeration"):
                                # an anonymous nested composite: name it after its own DIE
                                # so that it can be looked up as a type of its own
                                try:
                                    target = die.get_DIE_from_attribute("DW_AT_type")
                                    type_v = "$anon_0x%08x" % target.offset
                                    self.typeShape.setdefault(
                                        type_v,
                                        {
                                            "DW_TAG_structure_type": "structure",
                                            "DW_TAG_union_type": "union",
                                            "DW_TAG_enumeration_type": "enumeration",
                                        }[target.tag],
                                    )
                                except Exception:
                                    pass
                            elif type is not None and "DW_FORM_ref2" == type.form:
                                # for 2 D array, I see wrong order with this type for armcc elf
                                re2D = re.compile(r"(\w+)\[(\d+)\]\[(\d+)\]")
                                if re2D.search(type_v):
                                    ty, n1, n2 = re2D.search(type_v).groups()
                                    type_v = "%s[%s][%s]" % (ty, n2, n1)
                            if "DW_AT_data_member_location" in die.attributes:
                                loc = die.attributes["DW_AT_data_member_location"]
                                loc_v = self.describe_attr_value(die, loc)
                                if "DW_OP_plus_uconst" in loc_v:
                                    loc_v = loc_v.split(" ")[1]
                            else:
                                loc_v = "0x00000000"
                            pos_v = self.die_location(die)
                            id_v = "0x%08x" % (parent_die.offset)
                            mem = {
                                "name": name_v,
                                "type": type_v,
                                "offset": loc_v,
                                "loc": pos_v,
                            }
                            if id_v not in members:
                                members[id_v] = []
                            members[id_v].append(mem)
                else:
                    parent = die.get_parent()
        return members

    def buildType(self, name_v, id_v, shape, loc_v):
        """Type entry of the composite DIE `id_v`: shape, byte size and member layout."""
        typ = {"name": name_v, "type": shape, "shape": shape, "loc": loc_v}
        size = self.typeSize.get(id_v)
        if size is not None:
            typ["size"] = size
        if "enumeration" == shape:
            return typ
        ms = self.members.get(id_v)
        if ms is None:
            return None
        ms.sort(key=lambda x: eval(x["offset"]))
        typ["members"] = ms
        return typ

    def parseTypes(self):
        self.members = self.parseMembers()
        members = self.members
        types = {}

        def register(name_v, typ):
            """Register `typ` unless the same name is already described better.

            armcc emits the same type in every CU that sees the header: the definition
            with all the members once, and a member-less forward declaration everywhere
            else. Whichever came last must not win.
            """
            old = types.get(name_v)
            if old is not None:
                if "members" in old and "members" not in typ:
                    return
                if "size" in old and "size" not in typ:
                    typ = dict(typ, size=old["size"])
                if "shape" in old and "shape" not in typ:
                    typ = dict(typ, shape=old["shape"])
            types[name_v] = typ

        for cu in self._dwarfinfo.iter_CUs():
            parent = cu.get_top_DIE()
            for die in cu.iter_DIEs():
                if die.get_parent() == parent:
                    parent = die
                if die.is_null():
                    parent = die.get_parent()
                    continue
                if die.tag in COMPOSITE_TAGS:
                    # a struct/union/enum used through its tag is not a typedef, but a
                    # variable can still be declared with it - register it under that name
                    name_v = _safe_DIE_name(die)
                    if name_v:
                        typ = self.buildType(name_v, "0x%08x" % die.offset, COMPOSITE_TAGS[die.tag],
                                             self.die_location(die))
                        if typ is not None:
                            register(name_v, typ)
                    continue
                if die.tag != "DW_TAG_typedef":
                    continue
                a = die.attributes
                if "DW_AT_name" not in a or "DW_AT_type" not in a:
                    continue
                name_v = self.describe_attr_value(die, a["DW_AT_name"]).split('"')[1]
                type_v = self.die_type_name(die) or ""
                pos_v = self.die_location(die)
                typ = {"name": name_v, "type": type_v, "loc": pos_v}
                try:
                    target = die.get_DIE_from_attribute("DW_AT_type")
                except Exception:
                    target = None
                if target is not None and target.tag in COMPOSITE_TAGS:
                    # a typedef to a named composite reports the tag name, to an anonymous
                    # one the kind itself - resolve both through the target DIE
                    sub = self.buildType(name_v, "0x%08x" % target.offset, COMPOSITE_TAGS[target.tag],
                                         pos_v)
                    if sub is not None:
                        typ["shape"] = sub["shape"]
                        if "size" in sub:
                            typ["size"] = sub["size"]
                        if "members" in sub:
                            typ["members"] = sub["members"]
                if "size" not in typ:
                    # a scalar typedef such as uint64_t has no composite to size it
                    size = self.die_byte_size(target)
                    if size is not None:
                        typ["size"] = size
                register(name_v, typ)

        # resolve the anonymous composites the registered members and variables point at
        pending = set()
        for typ in types.values():
            for mem in typ.get("members", []):
                name_v = _strip_qualifiers(mem["type"])
                if name_v.startswith(ANON_PREFIX) and name_v not in types:
                    pending.add(name_v)
        for name_v in self.typeShape:
            if name_v.startswith(ANON_PREFIX) and name_v not in types:
                pending.add(name_v)
        while pending:
            name_v = pending.pop()
            id_v = name_v[6:]
            shape = self.typeShape.get(id_v, "structure")
            typ = self.buildType(name_v, id_v, shape, self.typeLoc.get(id_v, "unknown.c:0"))
            if typ is None:
                continue
            types[name_v] = typ
            for mem in typ.get("members", []):
                sub = _strip_qualifiers(mem["type"])
                if sub.startswith(ANON_PREFIX) and sub not in types:
                    pending.add(sub)
        return types

    def die_location(self, die):
        """`file:line` of a DIE, never raising on missing attributes."""
        attributes = die.attributes
        if "DW_AT_decl_file" in attributes:
            try:
                file_v = self.describe_attr_value(die, attributes["DW_AT_decl_file"])
                file_v = file_v.replace('"', "")
            except Exception:
                file_v = "unknown.c"
        else:
            file_v = "unknown.c"
        if "DW_AT_decl_line" in attributes:
            line_v = self.describe_attr_value(die, attributes["DW_AT_decl_line"])
        else:
            line_v = "0"
        return ":".join((file_v, line_v))

    def die_type_name(self, die):
        """Name of the type referenced by DW_AT_type, or None."""
        if "DW_AT_type" not in die.attributes:
            return None
        try:
            target = die.get_DIE_from_attribute("DW_AT_type")
        except Exception:
            target = None
        name = None
        try:
            name = self.describe_attr_value(die, die.attributes["DW_AT_type"]).split('"')[1].strip()
        except Exception:
            name = None
        # armcc points a qualified variable at a bare qualifier node, which renders as
        # the word `volatile` alone and hides the type it qualifies - walk into it
        qual = QUALIFIER_TAGS.get(getattr(target, "tag", None))
        if qual is not None:
            inner = self.die_type_name(target)
            name = ("%s %s" % (qual, inner)).strip() if inner else qual
        return name

    def anonCompositeName(self, die, depth=0):
        """Type name of a DIE, with unnamed composites replaced by `$anon_0x<offset>`.

        An unnamed struct renders as the bare word `structure`, which can never be
        looked up in the type table. The DIE it points at still has the members, so
        register it under a synthetic name and use that instead.
        """
        name_v = self.die_type_name(die)
        if name_v is None or depth > 32:
            return name_v
        m = re.match(r"^(structure|union|enumeration)\s*((?:\[\d+\])*)$", name_v.strip())
        if m is None:
            return name_v
        try:
            target = die.get_DIE_from_attribute("DW_AT_type")
        except Exception:
            return name_v
        while target is not None and depth <= 32:
            if target.tag in COMPOSITE_TAGS:
                anon = ANON_PREFIX + "%08x" % target.offset
                self.typeShape.setdefault(anon, COMPOSITE_TAGS[target.tag])
                self.typeLoc.setdefault(anon, self.die_location(target))
                return anon + m.group(2)
            if "DW_AT_type" not in target.attributes:
                break
            try:
                target = target.get_DIE_from_attribute("DW_AT_type")
            except Exception:
                break
            depth += 1
        return name_v

    def parseVars(self):
        vars = []
        self.varStats = collections.Counter()
        self.scanComposites()
        for cu in self._dwarfinfo.iter_CUs():
            cu_file = _safe_DIE_name(cu.get_top_DIE())
            if cu_file:
                cu_file = os.path.basename(cu_file.replace("\\", "/"))
            parent = cu.get_top_DIE()
            for die in cu.iter_DIEs():
                if die.get_parent() == parent:
                    parent = die
                if not die.is_null():
                    if (
                        die.tag == "DW_TAG_variable"
                        and "DW_AT_location" in die.attributes
                        and "DW_AT_name" in die.attributes
                    ):
                        # self.debugDIE(die)
                        self.varStats["candidates"] += 1
                        name_v = _safe_DIE_name(die)
                        loc = die.attributes["DW_AT_location"]
                        loc_v = self.describe_attr_value(die, loc)
                        # a global/static lives at a fixed address: one single DW_OP_addr
                        # and nothing else in the location expression
                        if "DW_OP_addr" not in loc_v or loc_v.count("DW_OP_") != 1:
                            self.varStats["not static"] += 1
                            continue
                        found = re.search(r"DW_OP_addr\s+(0x[0-9a-fA-F]+|\d+)", loc_v)
                        if found is None:
                            self.varStats["not static"] += 1
                            continue
                        addr = int(found.group(1), 0)
                        if 0 == addr:
                            # its section was garbage collected, it is not in the image
                            self.varStats["address == 0"] += 1
                            continue
                        if "DW_AT_type" not in die.attributes:
                            self.varStats["no DW_AT_type"] += 1
                            continue
                        type_v = self.anonCompositeName(die)
                        if not type_v:
                            self.varStats["no DW_AT_type"] += 1
                            continue
                        pos_v = self.die_location(die)
                        if pos_v.startswith("unknown.c") and cu_file:
                            pos_v = ":".join((cu_file, pos_v.split(":")[1]))
                        var = {
                            "name": name_v,
                            "addr": "0x%08x" % addr,
                            "type": type_v,
                            "loc": pos_v,
                        }
                        # the true byte size of this very DIE: the type table is keyed by
                        # name, so it cannot tell two file local types of the same name apart
                        size = self.die_byte_size(die)
                        if size is not None:
                            var["size"] = size
                        vars.append(var)
                        self.varStats["static"] += 1
                else:
                    parent = die.get_parent()
        return vars


STD_TYPES = {
    "int8_t": "U8",
    "int16_t": "U16",
    "int32_t": "U32",
    "uint8_t": "U8",
    "uint16_t": "U16",
    "uint32_t": "U32",
    "char": "U8",
    "short": "U16",
    "int": "U32",
    "long": "U32",
    "unsigned char": "U8",
    "unsigned short": "U16",
    "unsigned int": "U32",
    "unsigned long": "U32",
    "enumeration": "U32",
    "_Bool": "U8",
}

SCALAR_BITS = {
    "U8": 8,
    "U16": 16,
    "U32": 32,
}

# a single variable is not allowed to explode into more widgets than this
MAX_DATAS = 512


def _strip_qualifiers(typ):
    for qualifier in ("const", "volatile", "restrict", "__restrict"):
        typ = re.sub(r"\b%s\b" % qualifier, "", typ)
    return " ".join(typ.split())


def _split_array(typ):
    """'uint8_t[8]' -> ('uint8_t', [8]); 'char *[4]' -> ('char *', [4])."""
    m = re.match(r"^(.*?)\s*((?:\[\d+\])+)$", typ.strip())
    if m is None:
        return None
    base = m.group(1).strip()
    if not base:
        return None
    dims = [int(x) for x in re.findall(r"\[(\d+)\]", m.group(2))]
    if 0 == len(dims):
        return None
    return base, dims


def _join_array(base, dims):
    return base + "".join("[%d]" % d for d in dims)


def _int(v):
    try:
        return int(v, 0)
    except (TypeError, ValueError):
        return None


def getBaseType(types, typ):
    typ = _strip_qualifiers(typ)
    depth = 0
    while depth < 32:
        if typ in STD_TYPES:
            return STD_TYPES[typ]
        typdef = types.get(typ)
        if typdef is None:
            return None
        if "enumeration" == typdef.get("shape"):
            # an enum is as wide as its underlying type, which is not always 32 bits
            size = typdef.get("size")
            if size in (1, 2, 4):
                return {1: "U8", 2: "U16", 4: "U32"}[size]
        typ = _strip_qualifiers(typdef["type"])
        depth += 1
    return None


def isPointerType(types, typ):
    """True when `typ` is a pointer, possibly hidden behind typedefs."""
    typ = _strip_qualifiers(typ)
    if _split_array(typ) is not None:
        return False
    depth = 0
    while depth < 32:
        if "*" in typ:
            return True
        if typ not in types:
            return False
        typ = _strip_qualifiers(types[typ]["type"])
        depth += 1
    return False


def getTypeSize(types, typ, depth=0):
    """Byte size of a type, or None when it cannot be worked out."""
    if depth > 32:
        return None
    typ = _strip_qualifiers(typ)
    arr = _split_array(typ)
    if arr is not None:
        base, dims = arr
        count = 1
        for dim in dims:
            count *= dim
        size = getTypeSize(types, base, depth + 1)
        return None if size is None else size * count
    if isPointerType(types, typ):
        return 4
    baseType = getBaseType(types, typ)
    if baseType is not None:
        return SCALAR_BITS[baseType] // 8
    typdef = types.get(typ)
    if typdef is None:
        return None
    if typdef.get("size"):
        return typdef["size"]
    return getTypeSize(types, typdef.get("type", ""), depth + 1)


def _dataSize(data):
    typ = data["type"]
    if typ.endswith("Array"):
        typ = typ[:-5]
    bits = SCALAR_BITS.get(typ)
    if bits is None:
        return None
    return (bits // 8) * data.get("size", 1)


def _datasSize(datas):
    size = 0
    for data in datas:
        s = _dataSize(data)
        if s is None:
            return None
        size += s
    return size


def _arrayData(name, baseType, count, readonly=False):
    data = {
        "name": name,
        "type": baseType + "Array",
        "size": count,
        "display": "dec",
        "default": "%s" % ([0] * count),
    }
    if readonly:
        data["readonly"] = True
    return data


def _blobData(name, count, readonly=True, visible=True):
    """Raw bytes: either a field the tooling cannot describe, or alignment padding."""
    data = {
        "name": name,
        "type": "U8Array",
        "size": count,
        "display": "hex",
        "default": "%s" % ([0] * count),
    }
    if readonly:
        data["readonly"] = True
    if not visible:
        data["visible"] = False
    return data


def _expandTyped(types, name, typ, readonly, depth, labelPrefix=""):
    """Flatten a value of type `typ` named `name` into UI `datas`, None if impossible.

    `labelPrefix` is only used to disambiguate the fields of array elements, e.g.
    'lCanFrame_3.data'. It is empty for a plain member of a top level variable.
    """
    if depth > 32:
        return None
    if re.search(r"\bconst\b", typ):
        readonly = True
    typ = _strip_qualifiers(typ)
    # `labelPrefix` already ends with the element index, so a nameless element (an
    # opaque blob) keeps just the prefix instead of repeating the variable name
    label = "%s.%s" % (labelPrefix.rstrip("."), name) if name else labelPrefix.rstrip(".")
    arr = _split_array(typ)
    if arr is None and isPointerType(types, typ):
        # a pointer is a 32bit address: measurable, but calibrating it is rarely meant
        return [{"name": label, "type": "U32", "display": "hex", "default": 0, "readonly": True}]
    if arr is not None:
        base, dims = arr
        baseType = getBaseType(types, base)
        if baseType is not None:
            # plain array of scalars
            if 1 == len(dims):
                return [_arrayData(label, baseType, dims[0], readonly)]
            rows = 1
            for dim in dims[:-1]:
                rows *= dim
            return [_arrayData("%s_%d" % (label, i), baseType, dims[-1], readonly) for i in range(rows)]
        if 1 == len(dims) and isPointerType(types, base):
            return [_arrayData(label, "U32", dims[0], True)]
        # array of composites: expand element by element, indexing the field labels
        datas = []
        element = _join_array(base, dims[1:])
        for i in range(dims[0]):
            sub = _expandTyped(
                types, name, element, readonly, depth + 1, "%s%s_%d." % (labelPrefix, name, i)
            )
            if sub is None:
                return None
            datas.extend(sub)
            if len(datas) > MAX_DATAS:
                return None
        return datas
    baseType = getBaseType(types, typ)
    if baseType is not None:
        return [{"name": label, "type": baseType, "display": "dec", "default": 0}]
    datas = getDatas(types, typ, depth + 1, name, readonly, labelPrefix)
    if datas is not None:
        return datas
    size = getTypeSize(types, typ)
    if size is not None:
        # nothing is known about the fields, but the size is: keep the bytes
        return [_blobData(label, size, readonly)]
    return None


def getMemberData(types, mem, readonly=False):
    return _expandTyped(types, mem["name"], mem["type"], readonly, 0)


def getDatas(types, typ, depth=0, name="", readonly=False, labelPrefix=""):
    """Flatten a composite type into UI `datas`; gaps become hidden raw byte runs."""
    if depth > 32:
        return None
    typ = _strip_qualifiers(typ)
    typdef = types.get(typ)
    if typdef is None or ("members" not in typdef and "union" != typdef.get("shape")):
        return None
    total = typdef.get("size")
    if "union" == typdef.get("shape"):
        # union members overlap, the layout cannot be flattened field by field
        if total is None:
            return None
        return [_blobData(labelPrefix + name or typ, total, readonly)]
    members = typdef["members"]
    datas = []
    pos = 0
    for i, mem in enumerate(members):
        offset = _int(mem["offset"])
        if offset is None:
            return None
        if offset > pos:
            datas.append(_blobData(labelPrefix + "pad_%d" % pos, offset - pos, visible=False))
            pos = offset
        sub = _expandTyped(types, mem["name"], mem["type"], readonly, depth + 1, labelPrefix)
        if sub is None:
            # a field the tooling cannot describe: keep it as raw bytes when sized
            if i + 1 < len(members):
                nxt = _int(members[i + 1]["offset"])
            else:
                nxt = total
            if nxt is None or nxt <= offset:
                return None
            sub = [_blobData(labelPrefix + mem["name"], nxt - offset)]
        size = _datasSize(sub)
        if size is None:
            return None
        datas.extend(sub)
        pos = offset + size
        if len(datas) > MAX_DATAS:
            return None
    if total is not None and pos < total:
        datas.append(_blobData(labelPrefix + "pad_%d" % pos, total - pos, visible=False))
    if any("Array" in d["type"] for d in datas):
        for d in datas:
            d["newline"] = True
    return datas


def processXcp(info, stats=None):
    """Turn the raw DWARF variables into the per source file `xcp_vars` tree.

    Every static/global variable the ELF still contains is exported - there is no
    "at least two per file" or "no const" rule any more. A variable that cannot be
    described field by field is exported as raw bytes instead of being dropped, as
    long as its size is known.
    """
    types = info["types"]
    xcp_vars = {}
    for var in info["vars"]:
        name = var["name"]
        addr = var["addr"]
        addr = int(addr, 0) if isinstance(addr, str) else addr
        loc = var["loc"].replace("\\", "/")
        typ = var["type"]
        fileName = os.path.basename(loc.split(":")[0]) or "unknown.c"
        readonly = bool(re.search(r"\bconst\b", typ))
        ds = getMemberData(types, {"name": name, "type": typ}, readonly)
        blob = False
        size = var.get("size")
        if ds is not None and size is not None and _datasSize(ds) != size:
            # the flattened layout does not cover the variable's real bytes: two file
            # local types sharing one name cannot both live in the name keyed table, so
            # trust the size of the variable's own DIE and keep raw bytes instead
            ds = None
        if ds is None:
            if size is None:
                size = getTypeSize(types, typ)
            if size is not None:
                ds = [_blobData(name, size, readonly)]
                blob = True
        if stats is not None:
            stat = stats.setdefault(fileName, collections.Counter())
            stat["vars"] += 1
            if ds is None:
                stat["dropped"] += 1
            elif blob:
                stat["raw"] += 1
            elif readonly:
                stat["readonly"] += 1
        if ds is None:
            continue
        if fileName not in xcp_vars:
            xcp_vars[fileName] = []
        xcp_vars[fileName].append({"name": name, "addr": addr, "datas": ds})
    info["xcp_vars"] = xcp_vars


def report(elfParser, info, stats):
    stderr = sys.stderr
    stderr.write("readelf: %s\n" % (info["elf"],))
    vs = getattr(elfParser, "varStats", collections.Counter())
    stderr.write("  DW_TAG_variable with a name and a location : %d\n" % (vs["candidates"],))
    for reason in ("not static", "address == 0", "no DW_AT_type"):
        stderr.write("    skipped, %-28s : %d\n" % (reason, vs[reason]))
    stderr.write("  exported as `vars`                        : %d\n" % (len(info["vars"]),))
    stderr.write("  `xcp_vars`                                : %d entries in %d files\n" % (
        sum(len(v) for v in info["xcp_vars"].values()), len(info["xcp_vars"])))
    stderr.write("  %-24s %5s %8s %5s %7s\n" % ("file", "vars", "readonly", "raw", "dropped"))
    for name in sorted(stats, key=lambda k: (-stats[k]["vars"], k)):
        s = stats[name]
        stderr.write("  %-24s %5d %8d %5d %7d\n" % (name, s["vars"], s["readonly"], s["raw"], s["dropped"]))
    stderr.write("  %-24s %5d %8d %5d %7d\n" % (
        "TOTAL",
        sum(s["vars"] for s in stats.values()),
        sum(s["readonly"] for s in stats.values()),
        sum(s["raw"] for s in stats.values()),
        sum(s["dropped"] for s in stats.values())))


def main():
    parser = argparse.ArgumentParser(description="dump the static/global variables of an ELF into JSON")
    parser.add_argument("-i", "--input", help="input elf file", required=True)
    parser.add_argument("-o", "--output", help="output elf information json file", required=True)
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="print a per source file coverage summary to stderr",
    )
    args = parser.parse_args()
    elfParser = ElfParser(args.input)
    vars = elfParser.parseVars()
    vars.sort(key=lambda x: int(x["addr"], 0) if isinstance(x["addr"], str) else x["addr"])
    types = elfParser.parseTypes()
    info = {"elf": args.input, "vars": vars, "types": types, "endian": elfParser.getCpuEndian()}
    stats = collections.OrderedDict() if args.verbose else None
    processXcp(info, stats)
    with open(args.output, "w") as f:
        json.dump(info, f, indent=2)
    if args.verbose:
        report(elfParser, info, stats)


if __name__ == "__main__":
    main()
