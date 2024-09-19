local format, match, gmatch, tinsert = string.format, string.match, string.gmatch, table.insert

local json = require("dkjson")
local pdu = require("pdu")
local log = require("tch.logger").new("libat.sms",7)
local luapdu = require("luapdu")

local M = {}

function M.get_messages(device)
	local messages = {}
	local ret = device:send_sms_command("AT+CMGL=4", "+CMGL:")
	if ret then
		local multipart = {}
		for _,line in pairs(ret) do
			if line.response and line.pdu then
				local message = luapdu.decode(line.pdu)
				if message then
					local id, status = match(line.response, "+CMGL:%s?(%d+),%s?(%d+)")
					id = tonumber(id)
					if id then
						-- create key/value pairs as returned by the original pdu.so library
						message.number = (message.sender and message.sender.num) and message.sender.num or message.recipient and message.recipient.num or ""
						message.text = message.msg.content
						message.date = message.timestamp
						if status == "0" then
							message.status = "unread"
						else
							message.status = "read"
						end
						message.id = id
						log:notice('id="%d" pdu="%s" message=%s',message.id,line.pdu,json.encode(message))
						if message.msg and message.msg.has_udh and message.msg.udh.total_parts > 1 then
							local key = format("%s#%d",message.number,message.msg.udh.CSMS_reference)
							if not multipart[key] then
								multipart[key] = {
									total_parts = 0,
									parts_found = 0,
									parts = {},
								}
							end
							multipart[key].total_parts = message.msg.udh.total_parts
							multipart[key].parts_found = multipart[key].parts_found + 1
							multipart[key].parts[message.msg.udh.part_number] = message
							log:notice('id="%d" added to multipart cache key "%s"',message.id,key)
						else
							tinsert(messages, message)
							log:notice('id="%d" will be returned to caller',message.id)
						end
					end
				else
					log:error('Failed to decode "%s"',line.pdu)
				end
			end
		end
		local ids_to_delete = {}
		for key,ref in pairs(multipart) do
			log:notice('key="%s" total_parts=%d parts_found=%d',key,ref.total_parts,ref.parts_found)
			if ref.total_parts == ref.parts_found then
				local message
				for i=1,ref.total_parts do
					local part = ref.parts[i]
					if part then
						log:notice('id="%d" CSMS_reference="%d" part=%d/%d part=%s',part.id,part.msg.udh.CSMS_reference,part.msg.udh.part_number,part.msg.udh.total_parts,json.encode(part))
						if not message then
							message = {
								id = part.id,
								number = part.number,
								date = part.date,
								status = part.status,
								last_part = i,
							}
						else
							ids_to_delete[#ids_to_delete+1] = part.id
							if message.status == "read" and part.status == "unread" then
								message.status = part.status
							end
						end
						for prior=message.last_part+1,i-1 do
							message.text = format("%s[Part %s missing?]",message.text or "",prior)
						end
						message.text = (message.text or "") .. part.text
						message.last_part = i
					end
				end
				log:notice('id="%d" will be returned to caller: status="%s" number="%s" date="%s" text="%s" parts=%d',message.id,message.status,message.number,message.date,message.text,ref.total_parts)
				tinsert(messages, message)
			end
		end
		for i=1,#ids_to_delete do
			log:notice('id="%d" from multipart message deleted',ids_to_delete[i])
			M.delete(device,ids_to_delete[i])
		end
	end
	return { messages = messages }
end

function M.send(device, number, message)
	local pdu_parts,errMsg = luapdu.encode(number, message)
	local result
	if not errMsg then
		for _,pdu_str in ipairs(pdu_parts) do
			-- Set tpLayerLength to half (hex encoding) of string length and subtract 1 for default SMSC added in PDU library
			local tpLayerLength = ((#pdu_str/2) - 1)
			local sent = device:send_sms(format("AT+CMGS=%d", tpLayerLength), pdu_str, "+CMGS:", 60 * 1000)
			if sent then
				result = sent
			else
				result = nil
				errMsg = "Failed to send message"
				break
			end
		end
	end
	return result,errMsg
end

function M.delete(device, message_id)
	return device:send_command(format("AT+CMGD=%d", message_id))
end

function M.info(device)
	local info = {
		read_messages = 0,
		unread_messages = 0,
		max_messages = 0
	}
	local ret = device:send_singleline_command("AT+CPMS?", "+CPMS:")
	if ret then
		info.max_messages = tonumber(match(ret, '+CPMS:%s?".-",%s?%d+,%s?(%d+)'))
	end
	ret = M.get_messages(device)
	if ret then
		for _, message in pairs(ret.messages) do
			if message.status == "read" then
				info.read_messages = info.read_messages + 1
			elseif message.status == "unread" then
				info.unread_messages = info.unread_messages + 1
			end
		end
	end
	return info
end

function M.init(device)
	-- 3GPP TS 23.040, 3GPP TS 23.041 (messaging AT command syntax is compatible with GSM 07.05 Phase 2+)
	device:send_singleline_command("AT+CSMS=1", "+CSMS:")
	-- Enable de reception of SMS
	device:send_command("AT+CNMI=1,1,0,0,0")
	-- SMS PDU mode
	device:send_command("AT+CMGF=0")
	-- Select ME storage if available
	local ret = device:send_singleline_command('AT+CPMS=?', '+CPMS:')
	if ret then
		local storages = {}
		for section in gmatch(ret, '%(([A-Z0-9a-z ",/_]-)%)') do
			local store = {}
			for storage in gmatch(section, '([^,]+)') do
				storage = string.gsub(storage, '"', '')
				store[storage] = true
			end
			tinsert(storages, store)
		end
		if storages[1] and storages[1]["ME"] then
			device:send_singleline_command('AT+CPMS="ME","ME","ME"', '+CPMS:')
		else
			device:send_singleline_command('AT+CPMS="SM","SM","SM"', '+CPMS:')
		end
	end
end

return M
