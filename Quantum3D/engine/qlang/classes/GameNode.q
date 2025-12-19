class GameNode

    cptr NodePtr;

    method void GameNode()

        printf("GameNod constructed");

    end 


    method Vec3 GetPosition()

        printf("GetPosition");
        return new Vec3(1,2,3);

    end 

    method void OnPlay()

        printf("GameNode.OnPlay");

    end 

    method void OnUpdate()

        printf("GameNode.OnUpdate");

    end

    method void OnRender()

        printf("GameNode.OnRender");

    end 

    method void OnStop()

        printf("GameNode.OnStop");

    end 

    method void Turn(Vec3 rotation)

        printf("In GameNode.Turn");
        NodeTurn(NodePtr,rotation);

    end 



end 


printf("Game Node Registered");

