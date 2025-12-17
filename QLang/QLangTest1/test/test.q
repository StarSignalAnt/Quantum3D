class GameNode

    int32 x;
    int32 y;
    int32 frame;

    method void GameNode()
        x = 0
        y = 0
        frame = 0
        printf("GameNode constructor called")
    end

    method int32 Update(float32 deltaTime)
        x = x + 1
        y = y + 1
        frame = frame + 1
        printf("Update: deltaTime =", deltaTime, "frame =", frame, "position =", x, y)
        return 500
    end

    method void Render()
        printf("Render: Drawing at", x, y)
    end

end

class Player(GameNode)

    int32 health;
    string name;

    method void Player()
        health = 100
        name = "Hero"
        printf("Player constructor called - health:", health)
    end

    method void TakeDamage(int32 amount)
        health = health - amount
        printf("Player took damage:", amount, "- health now:", health)
    end

    method void Render()
        printf("Render Player:", name, "at", x, y, "with health", health)
    end

end
