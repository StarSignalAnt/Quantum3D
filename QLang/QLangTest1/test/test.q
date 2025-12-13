
class Node

    int32 val = 250;

end

class Test<T>

    T Val;
    Test<T> Next = null;



    method void Test(T node)

        Val = node;

        if Val !=null

            printf("T:",Val.val+1000+100);

        else

            printf("Val = null");

        end 


    end 

end 

Node n1 = new Node();

Test<Node> t1 = new Test<Node>(n1);


//3:18
