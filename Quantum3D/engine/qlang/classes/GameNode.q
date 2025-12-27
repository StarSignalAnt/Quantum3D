module Game
import Vec3;

class GameNode

    cptr NodePtr;

    method void GameNode()

        qprintf("GameNod constructed");
    

    end 




    method void Play()

        qprintf("GameNode.OnPlay");
        

    end 

    method void OnUpdate(float32 dt) virtual

        qprintf("GameNode.OnUpdate DT:%f",dt);
        
    

    end

    method void OnRender()

        qprintf("GameNode.OnRender");

    end 

    method void OnStop()

        qprintf("GameNode.OnStop");

    end 

    // Transform

    method void Turn(Vec3 rotation)

        Node_Turn(NodePtr,rotation);

    end 

   

end 


qprintf("Game Node Registered");

