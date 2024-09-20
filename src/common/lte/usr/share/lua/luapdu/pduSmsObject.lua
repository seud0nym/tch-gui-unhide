local bit = require("bit")
local pduString = require("luapdu.pduString")

local pduSmsObject = {}
pduSmsObject.__index = pduSmsObject

function pduSmsObject.new(content)
    local self = content
    setmetatable(self, pduSmsObject)
    return self
end

--! Create new TX sms object with default values
function pduSmsObject.newTx(recipientNum, message)
    if recipientNum and not recipientNum:match("^%+?%d+$") then
        error("Invalid recipient number! <"..recipientNum..">")
    end
    local content = {
            msgReference=0,
            recipient={
                num  = recipientNum or ""
            },
            protocol = 0,
            dcs = 0,
            --validPeriod = 0x10+11, -- 5 + 5*11 = 60 minutes (https://en.wikipedia.org/wiki/GSM_03.40)
            msg={
                multipart = false,
                content = message or ""
            }
        }
    return pduSmsObject.new(content)
end

--! Create new RX sms object with default values
function pduSmsObject.newRx(message)
    local content = {
            sender={
                num  = ""
            },
            protocol = 0,
            dcs = 0,
            timestamp = ("00"):rep(7),
            msg={
                multipart = false,
                content = message or ""
            }
        }
    return pduSmsObject.new(content)
end

function pduSmsObject:encode16bitPayload(content)
    local response = {}
    local length = 0

    while content:len() ~= 0 do
        -- http://lua-users.org/wiki/LuaUnicode
        -- X - octet1, Y - octet2
        local byte = content:byte(1)
        if     byte <= 0x7F then    -- 7bit
            response[#response+1] = "00"
            response[#response+1] = pduString:octet(byte)       -- 0b0XXXXXXX
            content = content:sub(2)
        elseif byte <= 0xDF then    -- 11bit
            local byte2 = content:byte(2)
            content = content:sub(3)
            local val = bit.lshift(bit.band(byte, 0x1F),6) +    -- 0b110XXXYY
                                   bit.band(byte2,0x3F)         -- 0b10YYYYYY
            response[#response+1] = pduString:octet(bit.rshift(val,8))
            response[#response+1] = pduString:octet(bit.band(val,0xFF))
        elseif byte <= 0xEF then    -- 16bit
            local byte2 = content:byte(2)
            local byte3 = content:byte(3)
            content = content:sub(4)
            local val = bit.lshift(bit.band(byte,  0x0F),12) +  -- 0b1110XXXX
                        bit.lshift(bit.band(byte2, 0x3F),6)  +  -- 0b10XXXXYY
                                   bit.band(byte3, 0x3F)        -- 0b10YYYYYY
            response[#response+1] = pduString:octet(bit.rshift(val,8))
            response[#response+1] = pduString:octet(bit.band(val,0xFF))
        else
            return error("Can't fit payload char into 16bit unicode!")
        end
        length = length + 2
    end
    return response, length
end

function pduSmsObject:encode7bitPayload(content,align)
    local encodeTable7bit = {["@"]=0,["£"]=1,["$"]=2,["¥"]=3,["è"]=4,["é"]=5,["ù"]=6,["ì"]=7,["ò"]=8,["Ç"]=9,["\n"]=10,["Ø"]=11,["ø"]=12,["\r"]=13,["Å"]=14,["å"]=15,["Δ"]=16,["_"]=17,["Φ"]=18,["Γ"]=19,["Λ"]=20,["Ω"]=21,["Π"]=22,["Ψ"]=23,["Σ"]=24,["Θ"]=25,["Ξ"]=26,["€"]=27,["Æ"]=28,["æ"]=29,["ß"]=30,["É"]=31,[" "]=32,["!"]=33,["\""]=34,["#"]=35,["¤"]=36,["%"]=37,["&"]=38,["'"]=39,["("]=40,[")"]=41,["*"]=42,["+"]=43,[","]=44,["-"]=45,["."]=46,["/"]=47,["0"]=48,["1"]=49,["2"]=50,["3"]=51,["4"]=52,["5"]=53,["6"]=54,["7"]=55,["8"]=56,["9"]=57,[":"]=58,[";"]=59,["<"]=60,["="]=61,[">"]=62,["?"]=63,["¡"]=64,["A"]=65,["B"]=66,["C"]=67,["D"]=68,["E"]=69,["F"]=70,["G"]=71,["H"]=72,["I"]=73,["J"]=74,["K"]=75,["L"]=76,["M"]=77,["N"]=78,["O"]=79,["P"]=80,["Q"]=81,["R"]=82,["S"]=83,["T"]=84,["U"]=85,["V"]=86,["W"]=87,["X"]=88,["Y"]=89,["Z"]=90,["Ä"]=91,["Ö"]=92,["Ñ"]=93,["Ü"]=94,["§"]=95,["¿"]=96,["a"]=97,["b"]=98,["c"]=99,["d"]=100,["e"]=101,["f"]=102,["g"]=103,["h"]=104,["i"]=105,["j"]=106,["k"]=107,["l"]=108,["m"]=109,["n"]=110,["o"]=111,["p"]=112,["q"]=113,["r"]=114,["s"]=115,["t"]=116,["u"]=117,["v"]=118,["w"]=119,["x"]=120,["y"]=121,["z"]=122,["ä"]=123,["ö"]=124,["ñ"]=125,["ü"]=126,["à"]=127}

    local bytes,response = {},{}
    local state = 0
    local carryover = 0
    local length = 0

    while content:len() ~= 0 or carryover ~= 0 do
        local char = content:sub(1,1)
        if content:len() ~= 0 then length = length + 1 end
        local charval = encodeTable7bit[char]
        content = content:sub(2)
        if charval == nil then charval = 0 end
        local val = bit.lshift(charval, state) + carryover
        if state~= 0 or content:len() == 0 then
            bytes[#bytes+1] = bit.band(val, 0xFF)
            carryover = bit.rshift(val, 8)
        else
            carryover = val
        end
        if state == 0 then state = 7 else state = state - 1 end
    end
    if align then
        local filler = 0
        for i = 1, #bytes do
          local next_filler = bit.rshift(bytes[i], 8 - align)
          bytes[i] = bit.lshift(bytes[i], align)
          bytes[i] = bit.bor(bytes[i], filler)
          bytes[i] = bit.band(bytes[i], 0xFF)
          filler = next_filler
        end
        if #bytes % 7 == 0 then
          bytes[#bytes+1] = filler
        end
    end
    for i = 1, #bytes do
        response[#response+1] = pduString:octet(bytes[i])
    end
    return response, length
end

function pduSmsObject:dcsEncodingBits()
    if     self.msg.content:match("[\196-\240]") then return 8
    elseif self.msg.content:match("[\128-\195]") then return 4
    else                                              return 0 end
end

function pduSmsObject:encodePayload(alphabetOverride)
    if alphabetOverride == nil then
        alphabetOverride = self:dcsEncodingBits()
    elseif alphabetOverride ~= 8 and
           alphabetOverride ~= 4 and
           alphabetOverride ~= 0 then
        error("Invalid alphabet override!")
    end

    local content
    if self.msg.multipart then content =  self.msg.parts
    else                       content = {self.msg.content} end

    local parts = {}
    local partNo = 1
    local refNo = math.floor(math.random()*255)
    for _,part in ipairs(content) do
        local header = { }
        local align
        if self.msg.multipart then
            self:encodeMultipartHeader(refNo,partNo,header)
            partNo = partNo + 1
        end
        local udh = table.concat(header)
        if self.msg.multipart then
            align = 7 - (#udh + 1) % 7
        end

        local text = { }
        local length = 0
        if alphabetOverride == 4 then
            while part:len() ~= 0 do
                local byte1 = part:byte(1)
                part = part:sub(2)
                if byte1 > 127 then
                    local byte2 = part:byte(1)
                    part = part:sub(2)
                    local asciiByte = bit.band( 0xFF, bit.rshift(byte1,6) + bit.band(byte2,0x3F))
                    text[#text+1] = pduString:octet(asciiByte)
                else
                    text[#text+1] = pduString:octet(byte1)
                end
                length = length + 1
            end
        elseif alphabetOverride == 8 then
            text,length = self:encode16bitPayload(part)
        elseif alphabetOverride == 0 then
            text,length = self:encode7bitPayload(part,align)
        else
            error("Unimplemented payload encoding alphabet!")
        end
        text = table.concat(text)

        if self.msg.multipart then
            length = length + #header + 1 -- Not sure why we have to add 1 here, but if we don't, last char gets dropped on decode
        end
        local partContent = {pduString:octet(length),udh,text}
        parts[#parts+1] = table.concat(partContent)
    end

    return parts
end

function pduSmsObject:contentProcessing()
    self.dcs = self:dcsEncodingBits()
    local contentLen = self.msg.content:len()
    local charWidth = 7
    if     self.dcs == 8 then charWidth = 16
    elseif self.dcs == 4 then charWidth =  8 end
    local maxChars = 140*8/charWidth
    self.msg.multipart = contentLen > maxChars
    if self.msg.multipart then
        maxChars = math.floor(134*8/charWidth)  -- 6 octets lost on multipart header
        self.type = bit.bor(0x40,self.type)     -- Set UDH-Indicator bit
        self.msg.partCount = math.ceil(contentLen/maxChars)
        if self.msg.partCount > 255 then
            error("Message content too long!")
            return
        end
        self.msg.parts = {}
        local content = self.msg.content
        while content:len() > 0 do
            self.msg.parts[#self.msg.parts+1] = content:sub(1,maxChars) --!todo >7bit characters take up more than 8 bits, so this vulgar method can accidentally split a LUA unicoded char in two
            content = content:sub(maxChars+1)
        end
    else
        self.type = bit.band(0xBF,self.type)    -- Unset UDH-Indicator bit
    end
end

function pduSmsObject:encodeMultipartHeader(refNo, partNo, response)
    -- User data header length
    response[#response+1] = pduString:octet(0x05)
    -- Information element identifier
    response[#response+1] = pduString:octet(0x00)
    -- Information element data length
    response[#response+1] = pduString:octet(0x03)
    -- SMS reference no (same for all parts)
    response[#response+1] = pduString:octet(refNo)
    -- SMS part amount
    response[#response+1] = pduString:octet(self.msg.partCount)
    -- SMS part index
    response[#response+1] = pduString:octet(partNo)
end

function pduSmsObject:numberType(number)
    if number:sub(1,1) == "+" then
        return 0x91
    else
        return 0xA1
    end
end

function pduSmsObject:encodeSMSC(response)
    if self.smsc and self.smsc.num then
        local rawSmscNumber = self.smsc.num:gsub("+","")
        self.smsc.len = math.ceil(rawSmscNumber:len()/2) + 1
        response[#response+1] = pduString:octet(self.smsc.len)
        self.smsc.type = self:numberType(self.smsc.num)
        response[#response+1] = pduString:octet(self.smsc.type)
        response[#response+1] = pduString:decOctets(rawSmscNumber)
    else
        response[#response+1] = pduString:octet(0x00)
    end
end

function pduSmsObject:encodeNumber(numObj, response)
    local rawNumber = numObj.num:gsub("+","")
    numObj.type = self:numberType(numObj.num)
    numObj.len  = rawNumber:len()
    response[#response+1] = pduString:octet(numObj.len)
    response[#response+1] = pduString:octet(numObj.type)
    response[#response+1] = pduString:decOctets(rawNumber)
end

function pduSmsObject:encodeRx(response)
    self.type = bit.band(0xFE, self.type)
    response[#response+1] = pduString:octet(self.type)
    -- Sender block
    self:encodeNumber(self.sender, response)
    -- Protocol
    response[#response+1] = pduString:octet(0x00)
    -- Data Coding Scheme https://en.wikipedia.org/wiki/Data_Coding_Scheme
    response[#response+1] = pduString:octet(self.dcs)
    -- Timestamp
    response[#response+1] = pduString:decOctets(("00"):rep(7))
    -- Payload
    local payload = self:encodePayload()
    local data = table.concat(response)
    response = {}
    for i,part in ipairs(payload) do
        response[i] = {data, part}
    end
    return response
end

function pduSmsObject:encodeTx(response)
    self.type = bit.bor(0x11, self.type)
    response[#response+1] = pduString:octet(self.type)
    -- Message reference
    response[#response+1] = pduString:octet(self.msgReference)
    -- Recipient block
    self:encodeNumber(self.recipient,response)
    -- Protocol
    response[#response+1] = pduString:octet(0x00)
    -- Data Coding Scheme https://en.wikipedia.org/wiki/Data_Coding_Scheme
    response[#response+1] = pduString:octet(self.dcs)
    -- Validity Period (96 hours)
    response[#response+1] = pduString:octet(0xAA)
    -- Payload
    local payload = self:encodePayload()
    local data = table.concat(response)
    response = {}
    for i,part in ipairs(payload) do
        response[i] = {data, part}
    end
    return response
end

function pduSmsObject:encode()
    local response = {}

    self.type = self.type or 0x00

    self:encodeSMSC(response)
    self:contentProcessing()

    if self.sender then
        response = self:encodeRx(response)
    elseif self.recipient then
        response = self:encodeTx(response)
    else
        error("No valid content!")
    end
    local pduParts = {}
    for i,sms in ipairs(response) do
       pduParts[i] = table.concat(sms)
    end
    return pduParts
end

return pduSmsObject