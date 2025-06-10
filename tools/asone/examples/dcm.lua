
function eval(s)
  local data = {}
  if "\"" == s:sub(1,1) then
    s = s:sub(2, -2)
  end
  if "text=" == s:sub(1, 5) then
    data = {}
    s = s:sub(6)
    local n = s:len()
    for i = 1, n, 1 do
      local c = s:sub(i, i)
      data[i] = c:byte()
    end
  else
    s = s:gsub("%[", "{")
    s = s:gsub("%]", "}")
    data = load("return" .. s)()
  end
  return data
end

---------------------------------------- DTC ---------------------------------------

local Environments = {
  { name="Battery", id=0x1001, type="uint16", display="dec", unit="v" },
  { name="VehileSpeed", id=0x1002, type="uint16", display="dec", unit="km/h" },
  { name="EngineSpeed", id=0x1003, type="uint16", display="dec", unit="r/min" },
  { name="Time", id=0x1004, type="struct", data={
      { name="year", type="uint8", display="hex" },
      { name="month", type="uint8", display="hex" },
      { name="day", type="uint8", display="hex" },
      { name="hour", type="uint8", display="hex" },
      { name="minute", type="uint8", display="hex" },
      { name="second", type="uint8", display="hex" }
    },
    unit="YY-MM-DD-HH"
  }
}

local ExtendedDatas= {}
ExtendedDatas[0x01] = {
  { name="Fault Occurance Counter", type="uint8", display="dec" },
  { name="Aging Counter", type="uint8", display="dec" },
  { name="Aged Counter", type="uint8", display="dec" }
}

local DtcNumberToName = {}
DtcNumberToName[0x112200]="DTC0"
DtcNumberToName[0x112201]="DTC1"
DtcNumberToName[0x112202]="DTC2"
DtcNumberToName[0x112203]="DTC3"
DtcNumberToName[0x112204]="DTC4"
DtcNumberToName[0x112205]="DTC_COMB0"

function decode_dtc_number(u32N, bitSize)
  local dctName = "unknown"
  return true, string.format("%s 0x%x", dctName, u32N[1])
end

local function decode_snapshot_data(desc, d, index)
  local l = -1
  local s = ''
  local v = 0
  if desc["type"] == "uint32" then
    v = (d[index]<<24) + (d[index+1]<<16) + (d[index+2]<<8) + d[index+3]
    l = 4
    s = string.format('%d(0x%x)', v, v)
  elseif desc["type"] == "uint16" then
    v = (d[index]<<8) + d[index+1]
    l = 2
    s = string.format('%d(0x%x)', v, v)
  else
    v = d[index]
    l = 1
    
  end

  if desc["display"] == "hex" then
    s = string.format('0x%x', v)
  else
    s = string.format('%d', v)
  end

  return l, s
end

local function decode_ext_data(desc, d, index)
  return decode_snapshot_data(desc, d, index)
end

function decode_snapshot(d)
  local headers = {}
  local values = {}
  local Idx = 1
  local len = rawlen(d)
  local index = 1
  local bOne = false
  while index < len do
    local recordNum = d[index]
    local recordLen = d[index+1]
    index = index + 2
    values[Idx] = string.format("%d", recordNum)
    if false == bOne then
      headers[Idx] = "Record Number"
    end
    Idx = Idx + 1
    for i = 1, recordLen, 1 do
      local id = (d[index]<<8) + d[index+1]
      index = index + 2
      local desc = nil
      for k, v in ipairs(Environments) do
        if v["id"] == id then
          desc = v
          break
        end
      end
      if desc == nil then
        return true, {"Error"}, {string.format("data with ID 0x%X not known", id)}, 0
      end
      if desc["name"] == "Time" and desc["type"] == "struct" then
        year = d[index]
        month = d[index+1]
        day = d[index+2]
        hour = d[index+3]
        minute = d[index+4]
        second = d[index+5]
        index = index + 6
        values[Idx] = string.format("%02x-%02x-%02x %02x:%02x:%02x", year, month, day, hour, minute, second)
        if false == bOne then
          headers[Idx] = "Date"
        end
        Idx = Idx + 1
      elseif desc["type"] == "struct" then
        for k, desc2 in ipairs(desc["data"]) do
          local l, v = decode_snapshot_data(desc2, d, index)
          index = index + l
          values[Idx] = v
          if false == bOne then
            headers[Idx] = desc2['name']
          end
          Idx = Idx + 1
        end
      else
        local l, v = decode_snapshot_data(desc, d, index)
        index = index + l
        values[Idx] = v
        if false == bOne then
          headers[Idx] = desc['name']
        end
        Idx = Idx + 1
      end
    end
    bOne = true
  end
  return true, headers, values, (index-1)*8
end

function decode_dtc_status(mask)
  local text = ''
  if (mask & 0x01) ~= 0 then
      text = text .. 'TF '
  end
  if (mask & 0x02) ~= 0 then
      text = text .. ' TFTOC'
  end
  if (mask & 0x04) ~= 0 then
      text = text .. ' PDTC'
  end
  if (mask & 0x08) ~= 0 then
      text = text .. ' CDTC'
  end
  if (mask & 0x10) ~= 0 then
      text = text .. ' TNCSLC'
  end
  if (mask & 0x20) ~= 0 then
      text = text .. ' TFSLC'
  end
  if (mask & 0x40) ~= 0 then
      text = text .. ' TNCTOC'
  end
  if (mask & 0x80) ~= 0 then
      text = text .. ' WIR'
  end
  return text
end

function decode_dtc_number_and_status(d)
  local headers = {"name", "DTC Number", "status", "text"}
  local values = {}
  local Idx = 1
  local len = rawlen(d)
  for i = 1, len, 4 do
    local name = "unknown"
    local dtcNumber = (d[i]<<16) + (d[i+1]<<8) + d[i+2]
    local mask = d[i+3]
    local text = decode_dtc_status(mask)
    if DtcNumberToName[dtcNumber] ~= nil then
      name = DtcNumberToName[dtcNumber]
    end
    values[Idx] = name
    values[Idx+1] = string.format("0x%x", dtcNumber)
    values[Idx+2] = string.format("0x%x", mask)
    values[Idx+3] = text
    Idx = Idx + 4
  end

  return true, headers, values, len*8
end


function decode_dtc_number_and_record_number(d)
  local headers = {"name", "DTC Number", "RecordNumber"}
  local values = {}
  local Idx = 1
  local len = rawlen(d)
  for i = 1, len, 4 do
    local name = "unknown"
    local dtcNumber = (d[i]<<16) + (d[i+1]<<8) + d[i+2]
    local recordNum = d[i+3]
    if DtcNumberToName[dtcNumber] ~= nil then
      name = DtcNumberToName[dtcNumber]
    end
    values[Idx] = name
    values[Idx+1] = string.format("0x%x", dtcNumber)
    values[Idx+2] = string.format("%d", recordNum)
    Idx = Idx + 3
  end

  return true, headers, values, len*8
end

function decode_extended_data(d)
  local headers = {}
  local values = {}
  local Idx = 1
  local len = rawlen(d)
  local index = 1
  local bOne = false
  while index < len do
    local recordNum = d[index]
    index = index + 1
    values[Idx] = string.format("%d", recordNum)
    if false == bOne then
      headers[Idx] = "Record Number"
    end
    Idx = Idx + 1
    if ExtendedDatas[recordNum] == nil then
      return true, {"Error"}, {string.format("unknown record number %d", recordNum)}, len*8
    end
    recordLen = rawlen(ExtendedDatas[recordNum])
    for i = 1, recordLen, 1 do
      desc = ExtendedDatas[recordNum][i]
      local l, v = decode_ext_data(desc, d, index)
      index = index + l
      values[Idx] = v
      if false == bOne then
        headers[Idx] = desc['name']
      end
      Idx = Idx + 1
    end
    bOne = true
  end
  return true, headers, values, (index-1)*8
end

------------------------- AUTHENTICATION -------------------------------------------
require("crypto")

function tohex(data)
  s = ''
  for i, v in ipairs(data) do
    s = s .. string.format('%02X', v)
    if i > 0 and i % 32 == 0 then
      s = s .. '\n'
    end
  end
  return s
end

function load_keys(role)
  local private_key = nil
  local public_key = nil
  local private_key_asc = nil
  local public_key_asc = nil
  prikey = string.format('%sprivate_key.txt', role)
  pubkey = string.format('%spublic_key.txt', role)
  private_key_f = io.open(prikey, "rb")
  public_key_f = io.open(pubkey, "rb")
  if public_key_f == nil or private_key_f == nil then
    private_key, public_key = crypto.keygen(2048)
    private_key_asc = crypto.base64_encode(private_key)
    public_key_asc = crypto.base64_encode(public_key)
    private_key_f = io.open(prikey, "wb")
    public_key_f = io.open(pubkey, "wb")
    private_key_f:write(private_key_asc)
    public_key_f:write(public_key_asc)
  else
    private_key_asc = private_key_f:read()
    public_key_asc = public_key_f:read()
    private_key = crypto.base64_decode(private_key_asc)
    public_key = crypto.base64_decode(public_key_asc)
  end

  private_key_f:close()
  public_key_f:close()

  return private_key, public_key, private_key_asc, public_key_asc
end


function load_certificate()
  private_key, public_key, private_key_asc, public_key_asc = load_keys('')
  ca_private_key, ca_public_key, ca_private_key_asc, ca_public_key_asc = load_keys('ca_')
  -- simuate a certificate issued by CA, the signature signed by CA's private key, nobody will know
  -- CA's private key but know CA's public key
  signature = crypto.sign(ca_private_key, public_key)
  signature_asc = crypto.base64_encode(signature)
  headers = { "public key", "signature" }

  -- dd = { 1, 2, 3}
  -- print("org:", tohex(dd))
  -- ens = crypto.encrypt(public_key, dd)
  -- print("ens:", tohex(ens))
  -- des = crypto.decrypt(private_key, ens)
  -- print("des:", tohex(des))

  data = {}
  len = rawlen(public_key)
  data[1] = (len >> 8) & 0xFF
  data[2] = len & 0xFF
  offset = 3
  for _, v in ipairs(public_key) do
    data[offset] = v
    offset = offset + 1
  end
  -- Diagnostic Role
  -- Unlocked Services
  len = rawlen(signature)
  data[offset] = (len >> 8) & 0xFF
  data[offset + 1] = len & 0xFF
  offset = offset + 2
  for _, v in ipairs(signature) do
    data[offset] = v
    offset = offset + 1
  end

  values = { public_key_asc, signature_asc }
  return data, headers, values
end

challenge = nil

function decode_challenge(d)
  headers = { "challenge" }
  values = { crypto.base64_encode(d) }
  challenge = d

  return true, headers, values, rawlen(d)*8
end

function sign_challenge()
  private_key, public_key, private_key_asc, public_key_asc = load_keys('')
  signature = crypto.sign(private_key, challenge)
  signature_asc = crypto.base64_encode(signature)
  headers = { "challenge", "signature" }
  values = {  crypto.base64_encode(challenge), signature_asc }
  return signature, headers, values
end


-- private_key, public_key, private_key_asc, public_key_asc = load_keys('')
-- signature = crypto.sign(private_key, public_key)
-- print('signature')
-- print(tohex(signature))
-- print(crypto.verify(public_key, public_key, signature))

-- require("x509")

-- rca = x509.open('D:/repository/ssas/build/nt/GCC/one/astrust/AS_Root_CA.der')
-- rkey = rca:public_key()
-- print('root public key')
-- print(tohex(rkey))

-- ca = x509.open('D:/repository/ssas/build/nt/GCC/one/astrust/astrust-rsa-ica1/AS_RSA_ICA1.der')

-- print('tbs')
-- tbs = ca:tbs_certificate()
-- print(tohex(tbs))

-- sig = ca:signature()
-- print('signature')
-- print(tohex(sig))

-- print(crypto.verify(rkey, tbs, sig))