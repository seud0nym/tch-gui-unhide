package.path = "./?.lua;" .. package.path

pdu = require("luapdu")
json = require("dkjson")

result,errmsg = pdu.encode("+61411512272","CRAZYTEL: Never share this code with anyone. If someone contacts you asking for this code, do not provide it to them. To log into your CRAZYTEL account,please use the code: 483984. If you did not request this code, please call us immediately at: 1800 272 998.")

print("07911614786007F0640ED04369509BA51699000042903112046504A00500032F020186D2A0364B2D32752067D95E9683E6E8B0BC0CA2A3D373D0F84D2E83EE693A1A1476E7DFEEB20B943483E6EF76F9ED2E83C66F373D3CA6CF41F9771D149EAFD3EE33C8FC9683E8E8F41C347E93CB2C10F90D72BFE920B8FC6D4F93CBA0341D447F83E8E872DB05A2BE41ECF7199476D3DFA0FCBB2E070DA5416D965A6482C2E3F1BBEEA6B340")

if errmsg then
    print("FAILED",errmsg)
else
    for k,v in ipairs(result) do
        print(v)
        msg,err = pdu.decode(v)
        if err then
            print("FAILED",errmsg)
        else
            print(json.encode(msg,{indent=true}))
        end
    end
end

result,errmsg = pdu.decode("07911614786007F0240B911614512172F20000429081011393040AD4F29C0E0A9FC36937")

if errmsg then
    print("FAILED",errmsg)
else
    print(json.encode(result,{indent=true}))
end