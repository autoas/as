require('can')

function eval(s)
  local data = {}
  if "\"" == s:sub(1, 1) then
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

local lNode = nil
local lTxId = 0x7A0
local lRxId = 0x7B0

function xcp_init(busid, txid, rxid)
  lNode = can.open(busid)
  lTxId = txid
  lRxId = rxid
  print(string.format('Xcp Lua Init, TxId = %X, RxId = %X, %s', lTxId, lRxId, lNode))
end

function decode_xcp_res(data, bitSize)
  local sres = ''
  local resource = data[1]
  if resource & 0x01 ~= 0 then
    sres = sres .. 'CALPAG '
  end
  if resource & 0x04 ~= 0 then
    sres = sres .. 'DAQ '
  end
  if resource & 0x08 ~= 0 then
    sres = sres .. 'STIM '
  end
  if resource & 0x10 ~= 0 then
    sres = sres .. 'PGM '
  end
  return true, sres
end

local lSeedMode = 0
local lSeedRes = 0
local lSeeds = { 0, 0, 0, 0 }

function record_seed_mode(data, bitSize)
  lSeedMode = data[1]
  return true, ''
end

function record_seed_resource(data, bitSize)
  lSeedRes = data[1]
  return true, ''
end

function record_seeds(data)
  local len = rawlen(data)
  local headers = { 'length', 'seed' }
  local str = ''
  lSeeds = {}
  for i = 2, len, 1 do
    str = str .. string.format('%02X ', data[i])
    lSeeds[i - 1] = data[i]
  end
  local values = { string.format('%d', data[1]), str }
  return true, headers, values, rawlen(data) * 8
end

function calculate_keys()
  local keys = {}
  local headers = { 'length', 'keys' }
  local str = ''
  local seed = (lSeeds[1] << 24) + (lSeeds[2] << 16) + (lSeeds[3] << 8) + (lSeeds[4])
  local key = seed ~ 0x78934673
  local keys = { 4, (key >> 24) & 0xFF, (key >> 16) & 0xFF, (key >> 8) & 0xFF, key & 0xFF }
  for i = 1, 4, 1 do
    str = str .. string.format('%02X ', keys[i + 1])
  end
  local values = { '4', str }
  return keys, headers, values
end

local lUploadLength = 0

function record_upload_length(data, bitSize)
  lUploadLength = data[1]
  return true, ''
end

function decode_upload_data(data)
  local olen = rawlen(data)
  local len = olen
  local headers = { 'Addr' }
  local values = {}
  while len < lUploadLength do
    r, canid, d = lNode:read(lRxId, 100)
    if true == r then
      for i = 2, rawlen(d), 1 do
        data[len + 1] = d[i]
        len = len + 1
      end
    else
      return false, { "Timeout Error: " .. tohex(data) }, {}, 0
    end
  end
  local offset = 1
  for i = 1, lUploadLength, 1 do
    if i < 33 then
      headers[i + 1] = string.format('%d', i - 1)
    end
    if (i - 1) % 32 == 0 then
      values[offset] = string.format('%02X', i - 1)
      offset = offset + 1
    end
    values[offset] = string.format('%02X', data[i])
    offset = offset + 1
  end
  return true, headers, values, olen * 8
end

local lDownloadTotalLength = 0
local lDownloadData = {}
local lDownloadOffset = 0

function prepare_download(data)
  lDownloadTotalLength = rawlen(data)
  lDownloadData = data;
  lDownloadOffset = 6
  if lDownloadOffset > lDownloadTotalLength then
    lDownloadOffset = lDownloadTotalLength
  end
  local str = string.format('%d,', lDownloadTotalLength)
  for i = 1, lDownloadOffset, 1 do
    str = str .. string.format('0x%02X,', data[i])
  end
  return true, str
end

function download_next(data)
  local olen = rawlen(data)
  local len = olen
  local headers = { 'packet' }
  local values = {}
  local packet = { 0xF0, lDownloadTotalLength }
  local str = string.format('F0 %02X ', lDownloadTotalLength)
  for i = 1, lDownloadOffset, 1 do
    str = str .. string.format('%02X ', lDownloadData[i])
    packet[2 + i] = lDownloadData[i]
  end

  local r = lNode:write(lTxId, packet)
  if true ~= r then
    return false, { "Tx Failed Error" }, { tohex(packet) }, 0
  end
  values[1] = str
  local vi = 2
  while lDownloadOffset < lDownloadTotalLength do
    lNode:sleep(10)
    packet = { 0xEF }
    local doSz = lDownloadTotalLength - lDownloadOffset
    if doSz > 6 then
      doSz = 6
    end
    packet[2] = doSz
    str = string.format('EF %02X ', doSz)
    for i = 1, doSz, 1 do
      packet[2 + i] = lDownloadData[lDownloadOffset + i]
      str = str .. string.format('%02X ', lDownloadData[lDownloadOffset + i])
    end
    values[vi] = str
    vi = vi + 1
    lDownloadOffset = lDownloadOffset + doSz
    r = lNode:write(lTxId, packet)
    if true ~= r then
      return false, { "Tx Failed Error" }, { tohex(packet) }, 0
    end
  end

  if rawlen(values) > 1 then
    r, canid, d = lNode:read(lRxId, 100)
    if true == r then
      if d[1] ~= 0xFF then
        return false, { "Slave response error: " .. tohex(d) }, {}, 0
      end
    else
      return false, { "Timeout Error: Failed to get slave response" }, {}, 0
    end
  end

  return true, headers, values, olen * 8
end

local lProgramTotalLength = 0
local lProgramData = {}
local lProgramOffset = 0

function prepare_program(data)
  lProgramTotalLength = rawlen(data)
  lProgramData = data;
  lProgramOffset = 6
  if lProgramOffset > lProgramTotalLength then
    lProgramOffset = lProgramTotalLength
  end
  local str = string.format('%d,', lProgramTotalLength)
  for i = 1, lProgramOffset, 1 do
    str = str .. string.format('0x%02X,', data[i])
  end
  return true, str
end

function program_next(data)
  local olen = rawlen(data)
  local len = olen
  local headers = { 'packet' }
  local values = {}
  local packet = { 0xD0, lProgramTotalLength }
  local str = string.format('0xD0 %02X ', lProgramTotalLength)
  for i = 1, lProgramOffset, 1 do
    str = str .. string.format('%02X ', lProgramData[i])
    packet[2 + i] = lProgramData[i]
  end
  local r = lNode:write(lTxId, packet)
  if true ~= r then
    return false, { "Tx Failed Error" }, { tohex(packet) }, 0
  end
  values[1] = str
  local vi = 2
  while lProgramOffset < lProgramTotalLength do
    lNode:sleep(10)
    packet = { 0xCA }
    local doSz = lProgramTotalLength - lProgramOffset
    if doSz > 6 then
      doSz = 6
    end
    packet[2] = doSz
    str = string.format('CA %02X ', doSz)
    for i = 1, doSz, 1 do
      packet[2 + i] = lProgramData[lProgramOffset + i]
      str = str .. string.format('%02X ', lProgramData[lProgramOffset + i])
    end
    values[vi] = str
    vi = vi + 1
    lProgramOffset = lProgramOffset + doSz
    r = lNode:write(lTxId, packet)
    if true ~= r then
      return false, { "Tx Failed Error" }, { tohex(packet) }, 0
    end
  end

  if rawlen(values) > 1 then
    r, canid, d = lNode:read(lRxId, 100)
    if true == r then
      if d[1] ~= 0xFF then
        return false, { "Slave response error: " .. tohex(d) }, {}, 0
      end
    else
      return false, { "Timeout Error: Failed to get slave response" }, {}, 0
    end
  end

  return true, headers, values, olen * 8
end

