local function positionState(guessIndex, answerIndex)
    return answerIndex - (answerIndex > guessIndex and 1 or 0)
end

local function easier(guess, answer)
    local states = {0, 0, 0, 0, 0}
    local used = {false, false, false, false, false}

    for index = 1, 5 do
        if string.byte(guess, index) == string.byte(answer, index) then
            states[index] = 5
            used[index] = true
        end
    end

    for guessIndex = 1, 5 do
        if states[guessIndex] == 0 then
            local letter = string.byte(guess, guessIndex)
            for answerIndex = 1, 5 do
                if not used[answerIndex] and letter == string.byte(answer, answerIndex) then
                    states[guessIndex] = positionState(guessIndex, answerIndex)
                    used[answerIndex] = true
                    break
                end
            end
        end
    end

    return states
end

return {
    check = easier
}
