local function leftfirst(guess, answer)
    local states = {0, 0, 0, 0, 0}
    local used = {false, false, false, false, false}

    for guessIndex = 1, 5 do
        local letter = string.byte(guess, guessIndex)
        if not used[guessIndex] and letter == string.byte(answer, guessIndex) then
            states[guessIndex] = 2
            used[guessIndex] = true
        else
            for answerIndex = 1, 5 do
                if not used[answerIndex] and letter == string.byte(answer, answerIndex) then
                    states[guessIndex] = 1
                    used[answerIndex] = true
                    break
                end
            end
        end
    end

    return states
end

return {
    check = leftfirst
}
