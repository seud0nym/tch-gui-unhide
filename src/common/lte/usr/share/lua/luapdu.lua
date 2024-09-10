-- Source: https://github.com/0x4C4A/lua-pdu

local pduString    = require('luapdu.string')

local function decodePduSms(pduSmsString)
    local pduStr = pduString.new(pduSmsString)
    return pduStr:decodePDU()
end

local luapdu = {
    _VERSION = "0.1",
    _DESCRIPTION = "LuaPDU : SMS PDU encoder/decoder",
    _COPYRIGHT = "Copyright (c) 2016 Linards Jukmanis <Linards.Jukmanis@0x4c4a.com>",
    decode = decodePduSms,
}

return luapdu
