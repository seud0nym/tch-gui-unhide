local bit = require("bit")

local pduString = {}
pduString.__index = pduString

function pduString.new(content)
    local self = setmetatable({},pduString)
    self.str = content or ""
    return self
end

function pduString:decodeOctet(str)
    if str:len() < 2 then error("Too short, can't get octet!") end
    return tonumber("0x"..str:sub(1,2)),str:sub(3)
end

function pduString:decodeDecOctets(str,count)
    if str:len() < count*2 then error("String too short!") end
    -- Flip the octets
    local result = ""
    local var = str:sub(1,count*2)
    while var:len() ~= 0 do
        result = result .. var:sub(1,2):reverse()
        var = var:sub(3)
    end
    -- Strip "F", if padded
    if result:sub(-1) == "F" then result = result:sub(1,-2) end
    return result,str:sub(count*2+1)
end

function pduString:octet(val)
    if val == nil then val = 0; error("NIL!")
    elseif val > 255  then error("Can't convert to octet - value too large!") end
    return bit.tohex(val,2):upper()
end

function pduString:decOctets(val)
    local response = {}
    val = val or ""
    if val:len() % 2 ~= 0 then val = val .. "F" end
    while val:len() ~= 0 do
        response[#response+1] = val:sub(2,2)
        response[#response+1] = val:sub(1,1)
        val = val:sub(3)
    end
    return table.concat(response)
end

function pduString:decodePayload(content,dcs,length,has_udh)
    local udh
    if has_udh then
        udh = {}
        udh.length = tonumber("0x"..content:sub(1,2))
        udh.content = content:sub(3,udh.length*2)
        content = content:sub(3+udh.length*2)
    end
    local dcsBits = bit.band(dcs,12)
    local payload
    if     dcsBits == 0  then payload = self:decode7bitPayload(content,length)
    elseif dcsBits == 4  then payload = self:decode8bitPayload(content,length)
    elseif dcsBits == 8  then payload = self:decode16bitPayload(content,length)
    elseif dcsBits == 12 then error("Invalid alphabet size!") end
    return payload,udh
end

function pduString:decode7bitPayload(content,length)
    local decodeTable7bit = {[0]="@",[1]="£",[2]="$",[3]="¥",[4]="è",[5]="é",[6]="ù",[7]="ì",[8]="ò",[9]="Ç",[10]="\n",[11]="Ø",[12]="ø",[13]="\r",[14]="Å",[15]="å",[16]="Δ",[17]="_",[18]="Φ",[19]="Γ",[20]="Λ",[21]="Ω",[22]="Π",[23]="Ψ",[24]="Σ",[25]="Θ",[26]="Ξ",[27]="€",[28]="Æ",[29]="æ",[30]="ß",[31]="É",[32]=" ",[33]="!",[34]="\"",[35]="#",[36]="¤",[37]="%",[38]="&",[39]="'",[40]="(",[41]=")",[42]="*",[43]="+",[44]=",",[45]="-",[46]=".",[47]="/",[48]="0",[49]="1",[50]="2",[51]="3",[52]="4",[53]="5",[54]="6",[55]="7",[56]="8",[57]="9",[58]=":",[59]=";",[60]="<",[61]="=",[62]=">",[63]="?",[64]="¡",[65]="A",[66]="B",[67]="C",[68]="D",[69]="E",[70]="F",[71]="G",[72]="H",[73]="I",[74]="J",[75]="K",[76]="L",[77]="M",[78]="N",[79]="O",[80]="P",[81]="Q",[82]="R",[83]="S",[84]="T",[85]="U",[86]="V",[87]="W",[88]="X",[89]="Y",[90]="Z",[91]="Ä",[92]="Ö",[93]="Ñ",[94]="Ü",[95]="§",[96]="¿",[97]="a",[98]="b",[99]="c",[100]="d",[101]="e",[102]="f",[103]="g",[104]="h",[105]="i",[106]="j",[107]="k",[108]="l",[109]="m",[110]="n",[111]="o",[112]="p",[113]="q",[114]="r",[115]="s",[116]="t",[117]="u",[118]="v",[119]="w",[120]="x",[121]="y",[122]="z",[123]="ä",[124]="ö",[125]="ñ",[126]="ü",[127]="à"}
    local data = {}
    local prevoctet = 0
    local state = 0
    local octet
    while content:len() ~= 0 and length ~= 0 do
        octet,content = self:decodeOctet(content)
        local val = bit.band(bit.lshift(octet,state),0x7F) + prevoctet
        prevoctet = bit.band(bit.rshift(octet,7-state),0x7F)
        data[#data+1] = decodeTable7bit[val]
        if state == 6 then
            data[#data+1] = decodeTable7bit[prevoctet]
            prevoctet = 0
            state = 0
            length = length - 2
        else
            length = length - 1
            state = state + 1
        end
    end
    if     length     ~= 0 then
        error("Content shorter than expected!")
    elseif content:len() ~= 0 then
        error("Content longer than expected!<"..content..">")
    end
    return table.concat(data)
end

function pduString:decode8bitPayload(content,length)
    local data = {}
    local octet = 0
    while content ~= "" and length ~=0 do
        octet,content = self:decodeOctet(content)
        data[#data+1] = string.char(octet)
        length = length - 1
    end
    if     length        > 0 then
        error("Content shorter than expected!")
    elseif content:len() > 0 then
        error("Content longer than expected!<"..content..">")
    end
    return table.concat(data)
end

function pduString:decode16bitPayload(content,length)
    local data = {}
    local octet1,octet2
    while self ~= "" and length ~= 0 do
        octet1,content = self:decodeOctet(content)
        octet2,content = self:decodeOctet(content)
        local val = bit.lshift(octet1,8) + octet2
        -- http://lua-users.org/wiki/LuaUnicode
        -- X - octet1,Y - octet2
        if val < 0x80 then       -- 7bit
            data[#data+1] = string.char(val)    -- 0b0XXXXXXX
        elseif val < 0x800 then  -- 11bit
            data[#data+1] = string.char(0xC0 + bit.band(bit.rshift(val,6),0x1F))   -- 0b110XXXYY
            data[#data+1] = string.char(0x80 + bit.band(val,0x3F))                 -- 0b10YYYYYY
        elseif val < 0x10000 then -- 16bit
            data[#data+1] = string.char(0xE0 + bit.band(bit.rshift(val,12),0x1F))  -- 0b1110XXXX
            data[#data+1] = string.char(0x80 + bit.band(bit.rshift(val,6),0x3F))   -- 0b10XXXXYY
            data[#data+1] = string.char(0x80 + bit.band(val,0x3F))                 -- 0b10YYYYYY
        end
        length = length - 2
    end
    if     length        ~= 0 then
        error("Content shorter than expected!<"..length..">")
    elseif content:len() ~= 0 then
        error("Content longer than expected!<"..content..">")
    end
    return table.concat(data):sub(-length)
end


function pduString:decodeTXmsg(content,response)
    response.msgReference,  content = self:decodeOctet(content)
    response.recipient = {}
    response.recipient.len, content = self:decodeOctet(content)
    response.recipient.type,content = self:decodeOctet(content)
    if response.recipient.len > 0 then
        local length = math.ceil(response.recipient.len/2)
        response.recipient.num,content = self:decodeDecOctets(content,length)
        if response.recipient.type == 0x91 then -- International format
            response.recipient.num = "+"..response.recipient.num
        end
    end
    response.protocol,   content = self:decodeOctet(content)
    response.dcs,   content = self:decodeOctet(content)
    if bit.band(response.type,0x18) ~= 0 then
        response.validity,   content = self:decodeOctet(content)
    end
    response.msg.len,    content = self:decodeOctet(content)
    response.msg.content,response.msg.udh = self:decodePayload(content,response.dcs,response.msg.len)
    return response
end

function pduString:decodeRXmsg(content,response)
    response.sender = {}
    response.sender.len, content = self:decodeOctet(content)
    response.sender.type,content = self:decodeOctet(content)
    if response.sender.len > 0 then
        local length = math.ceil(response.sender.len/2)
        response.sender.num,content = self:decodeDecOctets(content,length)
        if response.sender.type == 0x91 then -- International format
            response.sender.num = "+"..response.sender.num
        end
    end
    response.protocol,  content = self:decodeOctet(content)
    response.dcs,       content = self:decodeOctet(content)
    response.timestamp, content = self:decodeDecOctets(content,7)
    response.msg.len,   content = self:decodeOctet(content)
    response.msg.content,response.msg.udh = self:decodePayload(content,response.dcs,response.msg.len,response.msg.has_udh)
    return response
end

function pduString:decodePDU()
    local content = self.str
    local response = {smsc={},msg = { multipart = false,udh = false }}
    response.smsc.len,content = self:decodeOctet(content)
    if response.smsc.len > 0 then
        local smscNumLen = response.smsc.len - 1
        response.smsc.type, content = self:decodeOctet(content)
        response.smsc.num,  content = self:decodeDecOctets(content,smscNumLen)
        if response.smsc.type == 0x91 then -- International format
            response.smsc.num = "+"..response.smsc.num
        end
    else
        response.smsc = nil
    end
    response.type,content = self:decodeOctet(content)
    response.msg.multipart = bit.band(response.type,0x04) == 0
    response.msg.has_udh = bit.band(response.type,0x40) ~= 0
    local typeBits = bit.band(response.type,0x03)
    if     typeBits == 0 then
        return self:decodeRXmsg(content,response)
    elseif typeBits == 1 then
        return self:decodeTXmsg(content,response)
    else
        error("Unknown message type!")
    end
end

return pduString