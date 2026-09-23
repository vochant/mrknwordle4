local function wordle(guess, answer)
    local states = {0, 0, 0, 0, 0}
    local remaining = {}
    for index = 1, 5 do
        local actual = string.byte(answer, index)
        if string.byte(guess, index) == actual then
            states[index] = 2
        else
            remaining[actual] = (remaining[actual] or 0) + 1
        end
    end
    for index = 1, 5 do
        if states[index] == 0 then
            local letter = string.byte(guess, index)
            if (remaining[letter] or 0) > 0 then
                states[index] = 1
                remaining[letter] = remaining[letter] - 1
            end
        end
    end
    return states
end

local function mix(value)
    value = value + 0x9e3779b97f4a7c15
    value = (value ~ (value >> 30)) * 0xbf58476d1ce4e5b9
    value = (value ~ (value >> 27)) * 0x94d049bb133111eb
    return value ~ (value >> 31)
end

local function mixText(value, text)
    value = mix(value ~ #text)
    for index = 1, #text do value = mix(value ~ string.byte(text, index)) end
    return value
end

local function randomInt(seed, input, answer, step, lower, upper)
    local value = mixText(mixText(mix(seed ~ step), input), answer)
    return lower + value % (upper - lower + 1)
end

local seed = 0

local function modified(guess, answer)
    local states = wordle(guess, answer)
    local position = randomInt(seed, guess, answer, 0, 1, 5)
    local alternatives = {}
    for state = 0, 2 do
        if state ~= states[position] then table.insert(alternatives, state) end
    end
    states[position] = alternatives[randomInt(seed, guess, answer, 1, 1, 2)]
    return states
end

return {
    start = function()
        seed = math.random(0)
    end,
    finish = function()
        seed = 0
    end,
    check = function(guess, answer)
        return modified(guess, answer)
    end,
    compatible = function(guess, answer, feedback)
        local states = wordle(guess, answer)
        local differences = 0
        for index = 1, 5 do
            if states[index] ~= feedback[index] then differences = differences + 1 end
        end
        return differences == 1
    end
}
