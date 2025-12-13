// Test file for QLang core features
// Testing: comments, booleans, comparisons, this, local vars

/* Multi-line comment test
   This should be ignored by the tokenizer
*/

class other

    int32 check = 90;

    method void other()
        printf("other created");
    end 

    method int32 GetValue()
        return check;
    end 

    method void Value(int32 p1, int32 p2)
        // Test local variable
        int32 sum = p1 + p2;
        printf("Value params:", p1, p2, "sum:", sum);
    end 

end 

class Test

    int32 num = 43;
    string name = "Antony";
    bool active = true;
    other ot = new other();

    method void Test(int32 a,int32 b)

        printf("Test:",a,b);

    end 

    method void Test()
        // Constructor with this reference
        this.num = 100;
        printf("Test constructed, num:", this.num);
    end 

    method void TestMeth()
        printf("Testing Method!", num, name, ot.GetValue());
    end 

    method int32 TestMeth(int32 v,string name)

        printf("TestMethd:",v,name);        
        return 100;

    end 


end 

// Main program
Test t1 = new Test();

int32 val = 45;

t1.TestMeth();
val = 5;
printf("VAL:",val);

if val > 50

    printf("VAL>50");

elseif val>40

    printf("VAL>40");

else

    printf("VAL<40");

end 

for int32 x = 200 to 0 : -1


        printf("X:",x+-10);



next

float32 loop = 0;

while loop<100

    loop++;
    printf("LOOP:",loop);

wend 
