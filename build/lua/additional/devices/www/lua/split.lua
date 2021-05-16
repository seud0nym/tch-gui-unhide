--- split: string split function and iterator for Lua
--
-- Peter Aronoff
-- BSD 3-Clause License
-- 2012-2018
--
-- There are many split functions for Lua. This is mine. Though, actually,
-- I took lots of ideas and probably some code from the implementations on
-- the Lua-Users Wiki, http://lua-users.org/wiki/SplitJoin.
local find = string.find
local fmt = string.format
local cut = string.sub
local gmatch = string.gmatch
local error = error

--- Helper functions
--
-- Return a table composed of the individual characters from a string.
local explode = function (str)
  local t = {}
  for i=1, #str do
    t[#t + 1] = cut(str, i, i)
  end

  return t
end

--- split(string, delimiter) => { results }
-- Return a table composed of substrings divided by a delimiter or pattern.
local split = function (str, delimiter)
  -- Handle an edge case concerning the str parameter. Immediately return an
  -- empty table if str == ''.
  if str == '' then return {} end

  -- Handle special cases concerning the delimiter parameter.
  -- 1. If the pattern is nil, split on contiguous whitespace.
  -- 2. If the pattern is an empty string, explode the string.
  -- 3. Protect against patterns that match too much. Such patterns would hang
  --    the caller.
  delimiter = delimiter or '%s+'
  if delimiter == '' then return explode(str) end
  if find('', delimiter, 1) then
    local msg = fmt('The delimiter (%s) would match the empty string.',
                    delimiter)
    error(msg)
  end

  -- The table `t` will store the found items. `s` and `e` will keep
  -- track of the start and end of a match for the delimiter. Finally,
  -- `position` tracks where to start grabbing the next match.
  local t = {}
  local s, e
  local position = 1
  s, e = find(str, delimiter, position)

  while s do
    t[#t + 1] = cut(str, position, s-1)
    position = e + 1
    s, e = find(str, delimiter, position)
  end

  -- To get the (potential) last item, check if the final position is
  -- still within the string. If it is, grab the rest of the string into
  -- a final element.
  if position <= #str then
    t[#t + 1] = cut(str, position)
  end

  -- Special handling for a (potential) final trailing delimiter. If the
  -- last found end position is identical to the end of the whole string,
  -- then add a trailing empty field.
  if position > #str then
    t[#t + 1] = ''
  end

  return t
end

--- each(str, delimiter)
local each = function (str, delimiter)
  delimiter = delimiter or '%s+'
  if delimiter == '' then return gmatch(str, '.') end
  if find('', delimiter, 1) then
    local msg = fmt('The delimiter (%s) would match the empty string.',
                    delimiter)
    error(msg)
  end

  local s, e, subsection
  local position = 1
  local iter = function ()
    if str == '' then return nil end

    s, e = find(str, delimiter, position)
    if s then
      subsection = cut(str, position, s-1)
      position = e + 1
      return subsection
    elseif position <= #str then
      subsection = cut(str, position)
      position = #str + 2
      return subsection
    elseif position == #str + 1 then
      position = #str + 2
      return ''
    end
  end

  return iter
end

local first_and_rest = function(str, delimiter)
  delimiter = delimiter or '%s+'
  if delimiter == '' then return cut(str, 1, 1), cut(str, 2) end
  if find('', delimiter, 1) then
    local msg = fmt('The delimiter (%s) would match the empty string.',
                    delimiter)
    error(msg)
  end

  local s, e = find(str, delimiter)
  if s then
    return cut(str, 1, s - 1), cut(str, e + 1)
  else
    return str
  end
end

local version = function ()
  return '3.2.1'
end

local author = function ()
  return 'Peter Aronoff'
end

local url = function ()
  return 'https://github.com/telemachus/split'
end

local license = function ()
  return 'BSD 3-Clause'
end

return {
  split = split,
  each = each,
  spliterator = each,
  first_and_rest = first_and_rest,
  version = version,
  author = author,
  url = url,
  license = license,
}