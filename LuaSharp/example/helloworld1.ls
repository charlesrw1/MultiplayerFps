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
