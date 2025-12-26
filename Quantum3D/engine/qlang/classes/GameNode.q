module Game

class GameNode

    cptr NodePtr;

    method void GameNode()

        qprintf("GameNod constructed");

    end 

    method void OnPlay()

        qprintf("GameNode.OnPlay");

    end 

    method void OnUpdate(float32 dt)

        qprintf("GameNode.OnUpdate");

    end

    method void OnRender()

        qprintf("GameNode.OnRender");

    end 

    method void OnStop()

        qprintf("GameNode.OnStop");

    end 

   

end 


qprintf("Game Node Registered");

