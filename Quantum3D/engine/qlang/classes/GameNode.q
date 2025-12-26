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

    method void OnUpdate(float32 dt)

        qprintf("GameNode.OnUpdate DT:%f",dt);
        
        Vec3 turn = new Vec3(0.0,25.0*dt,0.0);
        Node_Turn(NodePtr,turn);


    end

    method void OnRender()

        qprintf("GameNode.OnRender");

    end 

    method void OnStop()

        qprintf("GameNode.OnStop");

    end 

   

end 


qprintf("Game Node Registered");

