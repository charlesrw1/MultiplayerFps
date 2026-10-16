class Entity {}

class MyClass {
private:
int integer;
float number;
Entity e = null;
Entity[] array = null;
table mytable = {};
table[] array_of_tables = null;

public:
MyClass() {
}

int myfunc(Entity e) {
    if (e != null) {
        return e.blah();
    }
    return 0;
}
}

auto m = new MyClass();
