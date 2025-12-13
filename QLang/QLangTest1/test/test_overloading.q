class Math {
    method int32 Add(int32 a, int32 b) {
        return a + b;
    }

    method string Add(string a, string b) {
        return a + b;
    }
}

class Person {
    int32 age = 0;
    string name = "Unknown";

    method Person() {
        age = 1;
        name = "Default";
    }

    method Person(string n, int32 a) {
        name = n;
        age = a;
    }
}

Math m = new Math();
int32 sum = m.Add(5, 10);
string concat = m.Add("Hello", " World");

print("Sum: " + sum);
print("Concat: " + concat);

Person p1 = new Person();
print("P1: " + p1.name + " " + p1.age);

Person p2 = new Person("Alice", 30);
print("P2: " + p2.name + " " + p2.age);
