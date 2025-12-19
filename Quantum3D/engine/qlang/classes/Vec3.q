class Vec3

    float32 X;
    float32 Y;
    float32 Z;

    method void Vec3()
    
        X=0;
        Y=0;
        Z=0;
        printf("Created default Vec3");


    end 

    method void Vec3(float32 x,float32 y,float32 z)

        X=x;
        Y=y;
        Z=z;
        printf("Created Vec3 ",x,y,z);

    end 

end 