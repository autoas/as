require('can')

------------------- COMMON API -----------------------
function eval(s)
    local value = load("return" .. s)()
    return value
end

function str2array(str)
    local data = {}
    local i = 1
    for s in string.gmatch(str, "%S+") do
        data[i] = eval('{0x' .. s .. '}')[1]
        i = i + 1
    end
    return data
end

function tohex(data)
    s = ''
    for i, v in ipairs(data) do
        s = s .. string.format('%02X ', v)
        if i > 0 and i % 32 == 0 then
            s = s .. '\n'
        end
    end
    return s
end

-------------------- DCM ---------------------------
local lSeed_01 = { 0x00, 0x00, 0x00, 0x00 }

function Security01Seed(res)
    local ret = 0
    local len = rawlen(res)

    if len == 6 and 0x67 == res[1] and 0x01 == res[2] then
        for i = 1, 4, 1 do
            lSeed_01[i] = res[2 + i]
        end
    else
        ret = -1
    end

    return ret
end

function Security01Key()
    local seed = (lSeed_01[1] << 24) + (lSeed_01[2] << 16) + (lSeed_01[3] << 8) + (lSeed_01[4])
    local key = seed ~ 0x78934673
    local req = { 0x27, 0x02, (key >> 24) & 0xFF, (key >> 16) & 0xFF, (key >> 8) & 0xFF, key & 0xFF }
    return 0, req
end

--------------------------- CANSM ---------------------

function TriggerBusOff(busid, address, expected)
    local ret = 0
    local msg = ''
    local node = can.open(busid)
    local borTime = tonumber(address)

    -- drain the message in canlib queue
    local r, canid, data = node:read(-1)
    while (r)
    do
        r, canid, data = node:read(-1)
    end

    -- simulate can busoff
    node:write(0xdeadbeef, { 0 })

    -- no messgae before bus of recovery time
    r, canid, data = node:read(-1, borTime - 100)
    if true == r then
        msg = string.format('Get msg %x %s before bus off recovery time', canid, tohex(data))
        ret = -1
    end

    -- see message after bus off recovery time
    r, canid, data = node:read(-1, 300)
    if true ~= r then
        msg = "no message after bus off recovery time"
        ret = -2
    end

    return ret, msg
end

----------------------------- XCP -----------------------------------
local lXcpNode = nil
local lXcpTxId = 0x7A0
local lXcpRxId = 0x7B0

function XcpEmptyRxQueue()
    local r, canid, res = lXcpNode:read(lXcpRxId, 100)
    while true == r do
        print("Xcp Unkown Response: ", tohex(res))
        r, canid, res = lXcpNode:read(lXcpRxId)
    end
end

function XcpRequest(req)
    local ret = 0
    XcpEmptyRxQueue()
    print("Xcp Request: ", tohex(req))
    lXcpNode:write(lXcpTxId, req)
    local r, canid, res = lXcpNode:read(lXcpRxId, 1000)
    while true == r do
        if 0xFF == res[1] then
            print("Xcp Response: ", tohex(res))
            break
        elseif 0xFE == res[1] then
            ret = -1
            print("Xcp Rx Error: ", tohex(res))
            break
        else
            print("Xcp Unkown Response: ", tohex(res))
            r, canid, res = lXcpNode:read(lXcpRxId, 1000)
        end
    end
    if false == r then
        ret = -1
        print("Timeout: no response for service: ", tohex(req))
    end

    return ret, res
end

function XcpConnect(busid, address, expected)
    local ret = 0
    local msg = ''
    lXcpNode = can.open(busid)
    v = eval('{' .. address .. '}')
    lXcpTxId = v[1]
    lXcpRxId = v[2]


    ret, res = XcpRequest({ 0xFF, 0x00 }) -- connect
    if 0 == ret then                      -- request seed
        ret, res = XcpRequest({ 0xF8, 0x00, 0x1D })
    end
    if 0 == ret then -- unlock
        local seed = (res[3] << 24) + (res[4] << 16) + (res[5] << 8) + (res[6])
        local key = seed ~ 0x78934673
        ret, res = XcpRequest({ 0xF7, 0x04, (key >> 24) & 0xFF, (key >> 16) & 0xFF, (key >> 8) & 0xFF, key & 0xFF })
    end

    return ret, msg
end

function XcpExecute(busid, request, expected)
    local ret = 0
    local msg = ''

    local req = str2array(request)
    ret, res = XcpRequest(req)
    if res ~= nil then
        msg = tohex(res)
        ret = 0
        local i = 1
        for gs in string.gmatch(expected, "%S+") do
            if gs ~= 'xx' and gs ~= 'XX' then
                v = eval('{0x' .. gs .. '}')[1]
                if v ~= res[i] then
                    ret = -1
                end
            end
            i = i + 1
        end
    end
    return ret, msg
end

function XcpSetDaq(busid, request, expected)
    local ret = 0
    local msg = ''

    local req = str2array(request)
    local daq, odt, entry, ext, len, addr = req[1], req[2], req[3], req[4], req[5], req[6]
    print(string.format('SetDaq: DAQ=%d ODT=%d Entry=%d Ext=%d Len=%d Address=0x%08X', daq, odt, entry, ext, len, addr))
    req = { 0xe2, 0x00, (daq >> 8) & 0xFF, daq & 0xFF, odt, entry }
    ret, _ = XcpRequest(req)
    if 0 ~= ret then
        msg = 'SetDat Failed'
    else
        req = { 0xe1, 0xff, len, ext, (addr >> 24) & 0xFF, (addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF }
        ret, _ = XcpRequest(req)
    end
    return ret, msg
end

function XcpCheckDaq0(busid, request, expected)
    local ret = 0
    local msg = ''
    local r, canid, res = lXcpNode:read(lXcpRxId, 100)
    while true == r do
        print('DAQ: ' .. tohex(res))
        msg = msg .. tohex(res) .. '\n'
        r, canid, res = lXcpNode:read(lXcpRxId, 100)
    end
    return ret, msg
end
