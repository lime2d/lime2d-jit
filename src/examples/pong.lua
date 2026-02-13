-- MAINSCRIPT
-- Pong for Lime2D (Two-Player)

-- Shorthand references
local lg = lime.graphics
local lk = lime.keyboard
local lw = lime.window
local lt = lime.time

-- Game constants
local PADDLE_WIDTH = 8
local PADDLE_HEIGHT = 50
local BALL_SIZE = 8
local PADDLE_SPEED = 280
local BALL_SPEED_INITIAL = 200
local BALL_SPEED_MAX = 450
local WINNING_SCORE = 11

-- Layout
local TOP_MARGIN = 24
local SIDE_MARGIN = 16

-- Game state
local left_paddle = {y = 0, score = 0}
local right_paddle = {y = 0, score = 0}
local ball = {x = 0, y = 0, vx = 0, vy = 0}
local ball_speed = BALL_SPEED_INITIAL
local serving = true
local serve_left = true
local game_over = false
local winner = ""

local function reset_ball()
    ball.x = lg.WIDTH / 2 - BALL_SIZE / 2
    ball.y = lg.HEIGHT / 2 - BALL_SIZE / 2
    ball_speed = BALL_SPEED_INITIAL

    local angle = (math.random() - 0.5) * math.pi / 2
    local dir = serve_left and -1 or 1
    ball.vx = math.cos(angle) * ball_speed * dir
    ball.vy = math.sin(angle) * ball_speed

    serving = true
end

local function init_game()
    math.randomseed(lt.sinceEpoch())
    local play_height = lg.HEIGHT - TOP_MARGIN
    left_paddle.y = TOP_MARGIN + play_height / 2 - PADDLE_HEIGHT / 2
    right_paddle.y = TOP_MARGIN + play_height / 2 - PADDLE_HEIGHT / 2
    left_paddle.score = 0
    right_paddle.score = 0
    game_over = false
    winner = ""
    serve_left = math.random() < 0.5
    reset_ball()
end

local function clamp_paddle(paddle)
    local min_y = TOP_MARGIN + 2
    local max_y = lg.HEIGHT - PADDLE_HEIGHT - 2
    if paddle.y < min_y then paddle.y = min_y end
    if paddle.y > max_y then paddle.y = max_y end
end

local function check_winner()
    if left_paddle.score >= WINNING_SCORE then
        game_over = true
        winner = "LEFT PLAYER"
    elseif right_paddle.score >= WINNING_SCORE then
        game_over = true
        winner = "RIGHT PLAYER"
    end
end

function lime.init()
    lw.setTitle("Lime2D - Pong")
    init_game()
end

function lime.update(dt)
    if game_over or serving then return end

    -- Left paddle controls (W/S)
    if lk.isDown(lk.KEY_W) then
        left_paddle.y = left_paddle.y - PADDLE_SPEED * dt
    end
    if lk.isDown(lk.KEY_S) then
        left_paddle.y = left_paddle.y + PADDLE_SPEED * dt
    end
    clamp_paddle(left_paddle)

    -- Right paddle controls (Up/Down)
    if lk.isDown(lk.KEY_UP) then
        right_paddle.y = right_paddle.y - PADDLE_SPEED * dt
    end
    if lk.isDown(lk.KEY_DOWN) then
        right_paddle.y = right_paddle.y + PADDLE_SPEED * dt
    end
    clamp_paddle(right_paddle)

    -- Move ball
    ball.x = ball.x + ball.vx * dt
    ball.y = ball.y + ball.vy * dt

    -- Top/bottom bounce
    if ball.y < TOP_MARGIN + 2 then
        ball.y = TOP_MARGIN + 2
        ball.vy = math.abs(ball.vy)
    end
    if ball.y + BALL_SIZE > lg.HEIGHT - 2 then
        ball.y = lg.HEIGHT - 2 - BALL_SIZE
        ball.vy = -math.abs(ball.vy)
    end

    -- Left paddle collision
    local left_x = SIDE_MARGIN
    if ball.vx < 0 and
       ball.x <= left_x + PADDLE_WIDTH and
       ball.x + BALL_SIZE >= left_x and
       ball.y + BALL_SIZE >= left_paddle.y and
       ball.y <= left_paddle.y + PADDLE_HEIGHT then

        ball.x = left_x + PADDLE_WIDTH
        ball.vx = math.abs(ball.vx)

        local relative_y = (ball.y + BALL_SIZE/2 - left_paddle.y) / PADDLE_HEIGHT
        ball.vy = ball.vy + (relative_y - 0.5) * 200

        ball_speed = math.min(BALL_SPEED_MAX, ball_speed + 15)
        local speed = math.sqrt(ball.vx * ball.vx + ball.vy * ball.vy)
        ball.vx = ball.vx / speed * ball_speed
        ball.vy = ball.vy / speed * ball_speed
    end

    -- Right paddle collision
    local right_x = lg.WIDTH - SIDE_MARGIN - PADDLE_WIDTH
    if ball.vx > 0 and
       ball.x + BALL_SIZE >= right_x and
       ball.x <= right_x + PADDLE_WIDTH and
       ball.y + BALL_SIZE >= right_paddle.y and
       ball.y <= right_paddle.y + PADDLE_HEIGHT then

        ball.x = right_x - BALL_SIZE
        ball.vx = -math.abs(ball.vx)

        local relative_y = (ball.y + BALL_SIZE/2 - right_paddle.y) / PADDLE_HEIGHT
        ball.vy = ball.vy + (relative_y - 0.5) * 200

        ball_speed = math.min(BALL_SPEED_MAX, ball_speed + 15)
        local speed = math.sqrt(ball.vx * ball.vx + ball.vy * ball.vy)
        ball.vx = ball.vx / speed * ball_speed
        ball.vy = ball.vy / speed * ball_speed
    end

    -- Scoring
    if ball.x < 0 then
        right_paddle.score = right_paddle.score + 1
        serve_left = true
        check_winner()
        reset_ball()
    elseif ball.x + BALL_SIZE > lg.WIDTH then
        left_paddle.score = left_paddle.score + 1
        serve_left = false
        check_winner()
        reset_ball()
    end

    lg.redraw()
end

function lime.draw()
    lg.clear(false)

    -- Draw court boundaries
    lg.lon(0, TOP_MARGIN, lg.WIDTH - 1, TOP_MARGIN)
    lg.lon(0, lg.HEIGHT - 1, lg.WIDTH - 1, lg.HEIGHT - 1)

    -- Draw center dashed line
    local cx = math.floor(lg.WIDTH / 2)
    for y = TOP_MARGIN + 4, lg.HEIGHT - 8, 16 do
        lg.ron(cx - 1, y, 3, 8)
    end

    -- Draw scores
    lg.locate(0, lg.COLS / 4 - 2)
    lg.print("P1: ")
    lg.printInt(left_paddle.score)

    lg.locate(0, lg.COLS * 3 / 4 - 3)
    lg.print("P2: ")
    lg.printInt(right_paddle.score)

    -- Draw paddles
    lg.ron(SIDE_MARGIN, math.floor(left_paddle.y), PADDLE_WIDTH, PADDLE_HEIGHT)
    lg.ron(lg.WIDTH - SIDE_MARGIN - PADDLE_WIDTH, math.floor(right_paddle.y), PADDLE_WIDTH, PADDLE_HEIGHT)

    -- Draw ball
    lg.ron(math.floor(ball.x), math.floor(ball.y), BALL_SIZE, BALL_SIZE)

    -- Draw controls hint
    lg.locate(0, 0)
    lg.print("W/S")
    lg.locate(0, lg.COLS - 5)
    lg.print("Up/Dn")

    -- Game state messages
    if game_over then
        lg.center("= " .. winner .. " WINS! =", lg.ROWS / 2, true)
        lg.center("Press R to rematch, ESC to quit", lg.ROWS / 2 + 2)
    elseif serving then
        lg.center("Press SPACE to serve", lg.ROWS - 2)
        local server = serve_left and "P1" or "P2"
        lg.center("(" .. server .. " serves)", lg.ROWS - 1)
    end
end

function lime.keypressed(key, scancode, isrepeat)
    if isrepeat then return end

    -- Global hotkeys
    if key == lk.KEY_X and lk.ctrlIsDown() then
        lw.quit(0)
        return
    end

    if key == lk.KEY_ESCAPE then
        lw.quit(0)
        return
    end

    if game_over then
        if key == lk.KEY_R then
            init_game()
        end
        return
    end

    if serving and key == lk.KEY_SPACE then
        serving = false
    end
end