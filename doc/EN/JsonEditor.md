---
layout: post
title: JsonEditor for SSAS
category: AUTOSAR
comments: true
---


# JsonEditor for SSAS

The **JsonEditor for SSAS** is a flexible, PyQt5-based configuration tool designed to streamline the setup of **SSAS** components. Inspired by the open-source [JSON Editor](https://json-editor.github.io/json-editor/) - which dynamically generates user interfaces from JSON schemas - this tool is **tailored specifically for automotive embedded systems**, with specialized features to simplify configuration of AUTOSAR Basic Software (BSW) modules such as DCM, COM, CANTP, Bootloader (BL), and more.

By combining a declarative schema with an intuitive GUI, the JsonEditor eliminates error-prone manual coding and accelerates development cycles in safety-critical automotive software projects.

---

## Key Features & Purpose

### 1. Schema-Driven GUI Generation  
Like its open-source counterpart, the JsonEditor for SSAS automatically renders a user-friendly graphical interface based on a provided **JSON schema**. This schema defines:
- Configuration structure (nested objects, arrays),
- Data types (boolean, integer, string, enum),
- Constraints (min/max, allowed values),
- Conditional visibility (`enabled` expressions),
- SSAS-specific extensions (e.g., `enumref`).

This enables non-programmers (e.g., system integrators) to safely configure complex BSW parameters without editing C code.


### 2. Automatic Configuration Export  
Once parameters are configured via the GUI, the tool **automatically generates C and header source files** (e.g., `Dcm_Cfg.c`, `Fls_Cfg.h`) that conform to AUTOSAR coding standards. This:
- Eliminates manual transcription errors,
- Ensures consistency between specification and implementation,
- Integrates seamlessly into build pipelines.

### 3. SSAS-Specific Enhancements  
While not fully compliant with the general [JSON Schema Specification](https://json-schema.org/), the tool includes **domain-specific extensions** critical for automotive use cases:
- **`enumref`**: References predefined enumeration lists (e.g., session names, security levels) to enforce valid selections.
- **Conditional enabling**: Fields can be shown/hidden based on other field values using expressions like `'${BL_USE_META}' == 'True'`.
- **Hexadecimal and expression support**: Accepts values like `0x731`, `8*1024`, or `0xA0600000 - 0xA0300000` for memory layout definitions.
- **Array-of-bank support**: Special handling for flash memory banks (`FlashA`, `FlashB`, `Fee`) with address/size/sectorSize tuples.

---

## Architecture Overview

The tool consists of three core components:

| Component | Role |
|--------|------|
| `main.py` | Entry point; parses CLI arguments (`-s schema.json`, `-c config.json`), initializes Qt app, loads schema/config, launches editor |
| `json_editor.py` | Core logic; recursively builds Qt tree widgets from schema, handles data binding, validation, and conditional UI updates |
| Schema (`schema.json`) | Declarative definition of configuration structure, types, defaults, and constraints |

The editor supports **two-way binding**: changes in the GUI update the internal JSON model, and saving exports it to C/H files via a backend generator (e.g., `Dcm.py`, `BL.py`).

---

## Quickstart: Using the JsonEditor for SSAS

### Step 1: Prepare a JSON Schema  
To start, you'll need a JSON schema that defines your SSAS configuration structure. You can either:  
- Use the [default SSAS schema](../../tools/json.editor/schema.json) (recommended for most use cases), or  
- Create a custom schema based on your project's requirements.  

For example, copy the schema from the [original JSON Editor demo](https://json-editor.github.io/json-editor/) (scroll to the bottom, click "Update Schema," then copy the JSON) and save it as `sc2.json`.  

![JSON Schema Example](../images/json-editor-schema-ex1.jpg)

### Step 2: Launch the JsonEditor for SSAS  
Navigate to the tool¡¯s directory and run the editor with your schema:  

```sh
# Launch with a custom schema (e.g., sc2.json)
cd tools/json.editor
python main.py -s sc2.json
```

A GUI window will open, rendering input fields, dropdowns, and other controls based on your schema. Configure parameters interactively (e.g., setting BSW module timeouts or PDU IDs).  

![JsonEditor Demo](../images/json-editor-example1.gif)

### Step 3: Generate SSAS Configuration Files  
After configuring parameters, save the settings. The tool automatically generates:  
- **C header/source files** (e.g., `Config.c`, `Config.h`) with the validated configuration values.  
- Documentation (optional) summarizing the configuration choices.  

---

## Running with the Default SSAS Schema  

For SSAS-specific projects, use the built-in `schema.json` to leverage pre-defined SSAS enumerations and constraints:  

```sh
# Launch with the default SSAS schema
cd tools/json.editor
python main.py
```

This loads the SSAS schema, enabling features like `enumref` for selecting predefined values.  

![SSAS JsonEditor Demo](../images/json-editor-ssas.gif)

---

## Supported Configuration Types (Examples)

The editor natively supports:
- **Booleans**: `BL_USE_AB`, `DCM_DISABLE_PROGRAM_SESSION_PROTECTION`
- **Integers/Hex**: `CAN_DIAG_P2P_RX = 0x731`, `FLASH_ERASE_SIZE = 512`
- **Strings with expressions**: `FINGER_PRINT_SIZE = "8*1024"`
- **Enums via `enumref`**: e.g., selecting `"Default"`, `"Program"` sessions from a predefined list
- **Arrays of objects**: e.g., `FlashA = [{ "address": "0x0", "size": "0x200000", "sectorSize": 512 }]`

Conditional logic ensures irrelevant fields are hidden (e.g., `META_SIZE` only appears if `ENABLE_META_SIZE_CONFIG` is true).

---

## Integration with AUTOSAR Toolchain

The JsonEditor is designed to plug into standard AUTOSAR workflows:
1. System architect defines schema (`schema.json`)
2. Integrator uses JsonEditor to create `Dcm.json`, `BL.json`, etc.
3. Generator scripts (`Dcm.py`, `Bl.py`) convert JSON ? C/H
4. Build system compiles generated code into BSW modules

This closes the loop between specification, configuration, and implementation.

---

## Troubleshooting Tips

- **Field not appearing?** Check the `enabled` condition in the schema—it may depend on another setting.
- **Invalid value rejected?** Ensure format matches expectations (e.g., hex must start with `0x`).
- **Crash on load?** Validate your JSON schema with a linter—malformed schemas may cause parsing errors.

---

The JsonEditor for SSAS bridges the gap between high-level system design and low-level embedded code, enabling faster, safer, and more maintainable automotive software development.