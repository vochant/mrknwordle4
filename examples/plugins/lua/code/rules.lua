return {
    judge = function(guess, answer)
        local result = {0, 0, 0, 0, 0}
        for index = 1, 5 do
            if string.byte(guess, index) == string.byte(answer, index) then
                result[index] = 2
            end
        end
        return result
    end
}
