-- Original Source: https://github.com/0x4C4A/lua-pdu

local pduSmsObject = require('luapdu.pduSmsObject')
local pduString    = require('luapdu.pduString')

local function printStackTrace(err)
    print(debug.traceback(err))
end

local function decodePduSms(pduSmsString)
    local pduStr = pduString.new(pduSmsString)
    local result
    local ok,errmsg = xpcall(function() result = pduStr:decodePDU() end,printStackTrace)
    if not ok then
        result = nil
    end
    return result,errmsg
end

local function encodeSmsPdu(recipient,content)
    local smsObj = pduSmsObject.newTx()
    smsObj.recipient.num = recipient
    smsObj.msg.content = content
    local pdu
    local ok,errmsg = xpcall(function() pdu = smsObj:encode() end,printStackTrace)
    if not ok then
        pdu = nil
    else
        errmsg = nil
    end
    return pdu,errmsg
end

local luapdu = {
    _VERSION = "0.1",
    _DESCRIPTION = "LuaPDU : SMS PDU encoder/decoder",
    _COPYRIGHT = "Copyright (c) 2016 Linards Jukmanis <Linards.Jukmanis@0x4c4a.com>",
    decode = decodePduSms,
    encode = encodeSmsPdu,
}

return luapdu
