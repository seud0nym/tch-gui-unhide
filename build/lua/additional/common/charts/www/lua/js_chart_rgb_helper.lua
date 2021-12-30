local TGU_Config = ngx.shared.TGU_Config

local colors = {
  night = {
    cpu = { "192,192,192","0,188,212",},
    ram = { "192,192,192","239,25,49",},
    wandn = { "192,192,192","0,255,0",},
    wanup = { "192,192,192","255,237,0",},
  },
  light = {
    cpu = { "128,128,128","0,0,255",},
    ram = { "128,128,128","255,0,0",},
    wandn = { "128,128,128","0,165,64",},
    wanup = { "128,128,128","255,152,0",},
  }
}

local M = {}

function M.getRGB(chart)
  local theme = TGU_Config:get("THEME") or ""
  local color = TGU_Config:get("COLOR") or ""

  if theme ~= "night" then
    theme = "light"
  end

  local index = 2
  if color == "MONOCHROME" then
    index = 1
  end

  return colors[theme][chart][index]
end

return M
