class GameNode

    cptr NodePtr;

    method void GameNode()

        printf("GameNod constructed");

    end 


    method Vec3 GetPosition()

        return NodeGetPosition(NodePtr);

    end 

    method void OnPlay()

        printf("GameNode.OnPlay");

    end 

    method void OnUpdate(float32 dt)

        printf("GameNode.OnUpdate");

    end

    method void OnRender()

        printf("GameNode.OnRender");

    end 

    method void OnStop()

        printf("GameNode.OnStop");

    end 

    method void Turn(Vec3 rotation)

        NodeTurn(NodePtr,rotation);

    end 

    method void SetPosition(Vec3 position)

        NodeSetPosition(NodePtr,position);

    end 


end 


printf("Game Node Registered");

