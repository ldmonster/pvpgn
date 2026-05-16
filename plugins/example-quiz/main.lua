--[[
    Example Quiz Plugin for PvPGN v3
    
    This plugin demonstrates:
    - Command registration
    - Event subscription
    - Chat messaging
    - Persistent storage
    - Database queries
]]--

-- Plugin initialization
function init()
    print("Quiz plugin initializing...")
    
    -- Register quiz commands
    pvpgn.commands.register("/quiz", handle_quiz_command, {
        group = "users",
        description = "Start or manage a quiz game"
    })
    
    -- Subscribe to user login events
    pvpgn.events.on("user_logged_in", function(event)
        local greeted = pvpgn.store.get("greeted." .. event.account_id)
        if not greeted then
            pvpgn.chat.send_whisper(event.account_id, event.account_id,
                "Welcome to the server! Type /quiz to start a quiz game.")
            pvpgn.store.put("greeted." .. event.account_id, true)
        end
    end)
    
    print("Quiz plugin initialized successfully")
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
        pvpgn.chat.send_whisper(account_id, account_id,
            "You already have a quiz in progress. Type /quiz stop to end it.")
        return false
    end
    
    -- Initialize quiz state
    pvpgn.store.put("quiz.state." .. account_id, {
        started = os.time(),
        score = 0,
        questions_answered = 0
    })
    
    pvpgn.chat.send_whisper(account_id, account_id,
        "Quiz started! You will be asked 10 questions. Good luck!")
    
    -- Send first question
    send_next_question(account_id)
    
    return true
end

-- Stop a quiz game
function stop_quiz(account_id)
    local quiz_state = pvpgn.store.get("quiz.state." .. account_id)
    
    if not quiz_state then
        pvpgn.chat.send_whisper(account_id, account_id,
            "You don't have a quiz in progress.")
        return false
    end
    
    -- Save final score
    local final_score = quiz_state.score
    pvpgn.store.put("quiz.final_score." .. account_id, final_score)
    
    -- Clear quiz state
    pvpgn.store.put("quiz.state." .. account_id, nil)
    
    pvpgn.chat.send_whisper(account_id, account_id,
        "Quiz ended! Your final score: " .. final_score)
    
    return true
end

-- Show top scores
function show_top_scores(account_id)
    pvpgn.chat.send_whisper(account_id, account_id,
        "Top 10 Quiz Scores:")
    pvpgn.chat.send_whisper(account_id, account_id,
        "1. Player1: 95 points")
    pvpgn.chat.send_whisper(account_id, account_id,
        "2. Player2: 87 points")
    pvpgn.chat.send_whisper(account_id, account_id,
        "3. Player3: 82 points")
    
    return true
end

-- Show quiz help
function show_quiz_help(account_id)
    pvpgn.chat.send_whisper(account_id, account_id,
        "Quiz Commands:")
    pvpgn.chat.send_whisper(account_id, account_id,
        "/quiz start - Start a new quiz")
    pvpgn.chat.send_whisper(account_id, account_id,
        "/quiz stop - Stop current quiz")
    pvpgn.chat.send_whisper(account_id, account_id,
        "/quiz top - Show top scores")
    
    return true
end

-- Send next question
function send_next_question(account_id)
    local quiz_state = pvpgn.store.get("quiz.state." .. account_id)
    
    if not quiz_state or quiz_state.questions_answered >= 10 then
        stop_quiz(account_id)
        return
    end
    
    -- Simple question
    local question_num = quiz_state.questions_answered + 1
    pvpgn.chat.send_whisper(account_id, account_id,
        "Question " .. question_num .. ": What is 2 + 2?")
    pvpgn.chat.send_whisper(account_id, account_id,
        "Type /answer <number> to answer")
end

-- Plugin shutdown
function shutdown()
    print("Quiz plugin shutting down...")
end

-- Call init when plugin loads
init()
