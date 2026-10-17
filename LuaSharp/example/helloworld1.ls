// Compile-time benchmark: exercises classes, enums, loops, tables, closures, and strings.

enum Shape {
    Circle,
    Square,
    Triangle = 10
}

enum Op {
    Add,
    Sub,
    Mul,
    Div
}

class Random {
    int state = 0;

    Random(int seed) {
        state = seed;
    }

    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) {
            state = -state;
        }
        return state;
    }

    int range(int low, int high) {
        return low + next() % (high - low + 1);
    }
}

class Point {
    int x = 0;
    int y = 0;

    Point(int px, int py) {
        x = px;
        y = py;
    }

    int manhattan(Point other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }

    int dot(Point other) {
        return x * other.x + y * other.y;
    }
}

class Calculator {
    int value = 0;
    int operations = 0;

    Calculator(int initial) {
        value = initial;
    }

    int apply(Op op, int amount) {
        operations += 1;
        if (op == Op.Add) {
            value = value + amount;
        } else if (op == Op.Sub) {
            value = value - amount;
        } else if (op == Op.Mul) {
            value = value * amount;
        } else if (amount != 0) {
            value = value % amount;
        }
        return value;
    }
}

class Stats {
    int count = 0;
    int total = 0;
    int minimum = 0;
    int maximum = 0;

    void add(int sample) {
        if (count == 0 || sample < minimum) { minimum = sample; }
        if (count == 0 || sample > maximum) { maximum = sample; }
        count += 1;
        total += sample;
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }
}

class Shapes {
    float area(Shape shape, float size) {
        if (shape == Shape.Circle) {
            return 3.14159 * size * size;
        } else if (shape == Shape.Square) {
            return size * size;
        }
        return size * size * 0.5;
    }
}

class Primes {
    bool isPrime(int n) {
        if (n < 2) { return false; }
        for (int d = 2; d * d <= n; d += 1) {
            if (n % d == 0) { return false; }
        }
        return true;
    }

    int countBelow(int limit) {
        int found = 0;
        for (int n = 0; n < limit; n += 1) {
            if (!isPrime(n)) { continue; }
            found += 1;
        }
        return found;
    }
}

class Fib {
    int iterative(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (true) {
            if (i >= n) { break; }
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int recursive(int n) {
        if (n < 2) { return n; }
        return recursive(n - 1) + recursive(n - 2);
    }
}

class Text {
    int score(string word) {
        int result = word.length();
        if (word.starts_with("hel")) { result += 10; }
        if (word.ends_with("ld")) { result += 20; }
        if (word.contains("o w")) { result += 40; }
        return result;
    }
}

printf("hello world\n");

auto rng = new Random(1234);
auto stats = new Stats();
for (int i = 0; i < 1000; i += 1) {
    stats.add(rng.range(1, 100));
}
printf("stats: count=%d min=%d max=%d mean=%f\n", stats.count, stats.minimum, stats.maximum, stats.mean());

auto origin = new Point(0, 0);
auto target = new Point(7, -3);
printf("points: manhattan=%d dot=%d\n", origin.manhattan(target), target.dot(new Point(2, 5)));

auto calc = new Calculator(10);
calc.apply(Op.Add, 5);
calc.apply(Op.Mul, 4);
calc.apply(Op.Sub, 7);
calc.apply(Op.Div, 9);
printf("calculator: value=%d ops=%d\n", calc.value, calc.operations);

auto shapes = new Shapes();
printf("areas: circle=%f square=%f triangle=%f\n", shapes.area(Shape.Circle, 2.0), shapes.area(Shape.Square, 3.0), shapes.area(Shape.Triangle, 4.0));

auto primes = new Primes();
printf("primes below 1000: %d\n", primes.countBelow(1000));

auto fib = new Fib();
printf("fib: iterative(40)=%d recursive(20)=%d\n", fib.iterative(40), fib.recursive(20));

auto text = new Text();
printf("text score: %d\n", text.score("hello world"));

table inventory = { name = "crate", ["weight"] = 12, [1] = new Point(1, 2) };
inventory["opened"] = true;
printf("inventory: %s opened=%b\n", "crate", true);

int base = 3;
auto scale = n => n * base;
auto hypot2 = (a, b) => a * a + b * b;
auto isEven = n => n % 2 == 0;
table log = {};
auto record = () => { log["done"] = 1; };
record();
printf("closures: scale=%d hypot2=%d even=%b\n", scale(14), hypot2(3, 4), isEven(10));

int evens = 0;
int odds = 0;
int checksum = 0;
for (int i = 0; i < 500; i += 1) {
    if (isEven(i)) {
        evens += 1;
    } else {
        odds += 1;
    }
    checksum = (checksum * 31 + i) % 1000003;
}
printf("loop: evens=%d odds=%d checksum=%d\n", evens, odds, checksum);

int[] integers;


for(i in 0..9) {
    integers.push_back(i);
}
printf("%d\n",integers.size());
for(i in integers) {
    printf("%d\n", i * 2);
}

string somestr = "Hello...";
somestr += "WORLD!\n";
printf(somestr);
int[] subintegers = {69,70,71};
for(i in subintegers) {
    printf("%d\n", i * 2);
}
printf("%d\n",integers[0]);

class Player {
    Player(int x) {

    }
    Player(string s) {
        name = s;
    }
    string player_func() {
        return "Player name:"+name;
    }
    void set_name(string s) {
        name = s;
    }

    Player[] otherplayers;
    string name = "none";


}
auto p = new Player("asdf");
p.set_name("charlie");
printf(p.player_func());
p.otherplayers.push_back(p);
printf("%d\n",p.otherplayers.size());
p.otherplayers.clear();

class Application {

    static float somefunc() {
        return 0.0;
    }
    table print_table(table t) {
        for(key,value in t) {
            printf("%s %s\n",key,value);
        }
        return t;
    }
}
auto app = new Application();
auto anothertable = app.print_table({key1="value1",key2="value2"});
printf("=================\n");
app.print_table(anothertable);

Vec2i vi = Vec2i(1,2);
Vec3i v3 = Vec3i(3,4,5);
vi +=Vec2i(v3.x,v3.z);
printf("%s",string(vi));

class Board {
    static int HELLO = 0;

    int get_board(Vec2i vec) {
        return positions[vec.x + vec.y*size.x];
    }
    void set_board(Vec2i vec, int i) {
        positions[vec.x + vec.y*size.x] = i;
    }
    void clear_board(Vec2i newsize, int value) {
        positions.clear();
        this.size = newsize;
        int total = size.x*size.y;
        for(i in 0..(total-1)) {
            positions.push_back(value);
        }
    }

    Vec2i size;
    int[] positions;
}

Board theboard = new Board();
theboard.clear_board(Vec2i(5,5),1);
printf("board vavlue: %d\n",theboard.get_board(Vec2i(0,0)));

Mat4x4 m = Mat4x4.identity();
m = m * Mat4x4.from_pos_rot(Vec3(), Quat());
m = m.inverse();

struct Mystruct {
    string s;
    Mat4x4 matrix;
    void function() {

    }
}
auto ms = Mystruct();
ms.function();



class Util {
    static bool contains(string s, string b) {
        return s.contains(b);
    }
}

/*
Wants:
debugger
ensuring program never segfaults/sandboxing
metrics about garbage collection, heap, etc.

dict, set type? function type? any type?

casting between types dynamically.

constructors of objects. super objects. virtual functions.

iterating properties of objects.

binding to C++ application...how?
    c++ will pass structs and pointers and lists and dicts and strings
    at script compilation time, have extern list of classes and structs
        these have field offsets and a table for getters/setters
    
    ClassBase system
    Struct system
    C++ objects not garbage collected? crap

    but want things like DynamicMaterial, DynamicModel to be GC'd
        or be careful. c++ made objects not GCd. LS objects GCd, so be very careful passing objects back and forth.
        never pass a LS object to C++
    

    .ls wrapper object that has ptr to c++ object
        calling .ls wrapper functions then calls c++ object. this is like what you have now,  kind of.
        {
        _ptr=0xffe34983
        get_position,
        set_position,
        get_parent,
        }
        ownership. 

    
    dont allow new to make c++ objects? must be done in c++.
    any `new` in .ls is GC managed. use handles or whatever.

    structs: have to copy...


table and list literals



include an optimized path for arrays

i.e. you can send ptrs to/from c++, somehow.
like for filling buffers for meshes or for transform data

can do raw memcpys of a struct defined in c++, or even not defined there.
copy data into the ptr with some range checks.
then C++ side can upload to a glbuffer

byte type.

then can do

raw_span<T> type:
    a view into a raw array of type T. 

T[] my_managed_array.
my_managed_array.to_raw_span(). creates a raw_span that c++ can read the ptr. 

can only make raw_span of a struct or literal type.

raw_spans are very dangerous. you shouldnt play around with them.

easy serialization.

slightly different lighter green syntax highlighting for struct/enum

*/