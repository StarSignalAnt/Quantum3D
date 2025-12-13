class other

    int32 check = 90;

    method void other()

        printf("other created");

    end 

    method int32 GetValue()

        return check;

    end 

    method void Value(int32 p1,int32 p2)

        printf("Value:",p1,p2);

    end 

end 

class Test

    int32 num=43;
    string name = "Antony";
    other ot = new other();

    method void Test()



    end 

    method void TestMeth()

            printf("Test constructed",num,name,ot.GetValue());

    end 

end 


Test t1 = new Test();

t1.ot.check = 50;

t1.TestMeth();

t1.ot.Value(25,75+t1.ot.check);

