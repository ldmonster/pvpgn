-- Lua API v2 (pvpgn.* namespace)
--[[
    Example Quiz Plugin for PvPGN v3
    
    This plugin demonstrates:
    - Command registration
    - Event subscription
    - Chat messaging
    - Persistent storage
    - Database queries
    
    Uses the pvpgn.* Lua API v2 namespace.
    For backward compatibility with old bnetd_* calls, load the legacy shim:
        require("legacy_shim")  -- or call install_legacy_shim() from C++
]]--

-- Plugin initialization
function init()
    pvpgn.log("info", "Quiz plugin initializing...")
    
    -- Register quiz commands
    pvpgn.commands.register("/quiz", handle_quiz_command, {
        group = "users",
        description = "Start or manage a quiz game"
    })
    
    -- Subscribe to user login events
    pvpgn.events.on("user_logged_in", function(event)
        local greeted = pvpgn.store.get("greeted." .. event.account_id)
        if not greeted then
            pvpgn.send_chat(event.account_id,
                "Welcome to the server! Type /quiz to start a quiz game.")
            pvpgn.store.put("greeted." .. event.account_id, true)
        end
    end)
    
    pvpgn.log("info", "Quiz plugin initialized successfully")
end

-- Handle /quiz command
function handle_quiz_command(session, args)
    local account_id = session.account_id
    local subcommand = args[1] or "help"
    
    if subcommand == "start" then
        return start_quiz(account_id)
    elseif subcommand == "stop" then
        return stop_quiz(account_id)
    elseif subcommand == "top" then
        return show_top_scores(account_id)
    else
        return show_quiz_help(account_id)
    end
end

-- Start a quiz game
function start_quiz(account_id)
    local quiz_state = pvpgn.store.get("quiz.state." .. account_id)
    
    if quiz_state then
        pvpgn.send_chat(account_id,
            "You already have a quiz in progress. Type /quiz stop to end it.")
        return false
    end
    
    -- Initialize quiz state
    pvpgn.store.put("quiz.state." .. account_id, {
        started = os.time(),
        score = 0,
        questions_answered = 0
    })
    
    pvpgn.send_chat(account_id,
        "Quiz started! You will be asked 10 questions. Good luck!")
    
    -- Send first question
    send_next_question(account_id)
    
    return true
end

-- Stop a quiz game
function stop_quiz(account_id)
    local quiz_state = pvpgn.store.get("quiz.state." .. account_id)
    
    if not quiz_state then
        pvpgn.send_chat(account_id,
            "You don't have a quiz in progress.")
        return false
    end
    
    -- Save final score
    local final_score = quiz_state.score
    pvpgn.store.put("quiz.final_score." .. account_id, final_score)
    
    -- Clear quiz state
    pvpgn.store.put("quiz.state." .. account_id, nil)
    
    pvpgn.send_chat(account_id,
        "Quiz ended! Your final score: " .. final_score)
    
    return true
end

-- Show top scores
function show_top_scores(account_id)
    local scores = pvpgn.store.get("quiz.top_scores") or {}
    
    if #scores == 0 then
        pvpgn.send_chat(account_id, "No scores recorded yet.")
        return
    end
    
    pvpgn.send_chat(account_id, "=== Top Quiz Scores ===")
    for i, entry in ipairs(scores) do
        if i > 10 then break end
        pvpgn.send_chat(account_id,
            i .. ". " .. entry.name .. ": " .. entry.score)
    end
end

-- Show quiz help
function show_quiz_help(account_id)
    pvpgn.send_chat(account_id, "Quiz commands:")
    pvpgn.send_chat(account_id, "  /quiz start  - Start a new quiz")
    pvpgn.send_chat(account_id, "  /quiz stop   - Stop current quiz")
    pvpgn.send_chat(account_id, "  /quiz top    - Show top scores")
end

-- Send the next question to the player
function send_next_question(account_id)
    local questions = get_questions()
    local quiz_state = pvpgn.store.get("quiz.state." .. account_id)
    
    if not quiz_state then return end
    
    local q_index = (quiz_state.questions_answered % #questions) + 1
    local question = questions[q_index]
    
    pvpgn.send_chat(account_id,
        "Question " .. (quiz_state.questions_answered + 1) ..
        ": " .. question.text)
end

-- Handle a quiz answer from a player
function on_quiz_answer(event)
    local account_id = event.account_id
    local answer = event.message
    local quiz_state = pvpgn.store.get("quiz.state." .. account_id)
    
    if not quiz_state then return end
    
    local questions = get_questions()
    local q_index = (quiz_state.questions_answered % #questions) + 1
    local question = questions[q_index]
    
    if answer:lower() == question.answer:lower() then
        quiz_state.score = quiz_state.score + 1
        pvpgn.send_chat(account_id, "Correct! Score: " .. quiz_state.score)
    else
        pvpgn.send_chat(account_id,
            "Wrong! The answer was: " .. question.answer)
    end
    
    quiz_state.questions_answered = quiz_state.questions_answered + 1
    
    if quiz_state.questions_answered >= 10 then
        -- Quiz complete
        local final_score = quiz_state.score
        pvpgn.store.put("quiz.final_score." .. account_id, final_score)
        pvpgn.store.put("quiz.state." .. account_id, nil)
        pvpgn.send_chat(account_id,
            "Quiz complete! Final score: " .. final_score .. "/10")
        
        -- Update leaderboard
        update_leaderboard(account_id, final_score)
    else
        pvpgn.store.put("quiz.state." .. account_id, quiz_state)
        send_next_question(account_id)
    end
end

-- Update the global leaderboard
function update_leaderboard(account_id, score)
    local account = pvpgn.get_account(account_id)
    if not account then return end
    
    local scores = pvpgn.store.get("quiz.top_scores") or {}
    table.insert(scores, { name = account.name, score = score })
    table.sort(scores, function(a, b) return a.score > b.score end)
    
    -- Keep only top 10
    while #scores > 10 do
        table.remove(scores)
    end
    
    pvpgn.store.put("quiz.top_scores", scores)
    
    -- Announce high score
    if scores[1] and scores[1].name == account.name then
        pvpgn.broadcast("", account.name .. " set a new quiz high score: " .. score .. "!")
    end
end

-- Sample quiz questions
function get_questions()
    return {
        { text = "What is the capital of France?",          answer = "Paris" },
        { text = "What is 2 + 2?",                          answer = "4" },
        { text = "What color is the sky?",                  answer = "blue" },
        { text = "How many days are in a week?",            answer = "7" },
        { text = "What is the largest planet?",             answer = "Jupiter" },
        { text = "What is H2O?",                            answer = "water" },
        { text = "How many continents are there?",          answer = "7" },
        { text = "What is the speed of light (approx km/s)?", answer = "300000" },
        { text = "Who wrote Romeo and Juliet?",             answer = "Shakespeare" },
        { text = "What is the chemical symbol for gold?",   answer = "Au" },
    }
end
