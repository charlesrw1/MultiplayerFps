
enum Mode0 {
    Idle,
    Run,
    Stop = 10
}

class Rng0 {
    int state = 0;
    Rng0(int seed) { state = seed + 0; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt0 {
    int x = 0;
    int y = 0;
    Pt0(int px, int py) { x = px; y = py; }
    int manhattan(Pt0 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt0 other) { return x * other.x + y * other.y; }
}

class Worker0 {
    int total = 0;
    int count = 0;
    Mode0 mode = Mode0.Idle;
    string name = "worker0";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode0.Stop; } else { mode = Mode0.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner0 {
    int run() {
    auto rng = new Rng0(0);
    auto worker = new Worker0();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt0(1, 2);
    auto b = new Pt0(4, -6);
    int bias = 0;
    auto scale = n => n * bias;
    table t = { name = "m0", ["n"] = 0 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode1 {
    Idle,
    Run,
    Stop = 10
}

class Rng1 {
    int state = 0;
    Rng1(int seed) { state = seed + 1; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt1 {
    int x = 0;
    int y = 0;
    Pt1(int px, int py) { x = px; y = py; }
    int manhattan(Pt1 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt1 other) { return x * other.x + y * other.y; }
}

class Worker1 {
    int total = 0;
    int count = 0;
    Mode1 mode = Mode1.Idle;
    string name = "worker1";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode1.Stop; } else { mode = Mode1.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner1 {
    int run() {
    auto rng = new Rng1(1);
    auto worker = new Worker1();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt1(1, 2);
    auto b = new Pt1(4, -6);
    int bias = 1;
    auto scale = n => n * bias;
    table t = { name = "m1", ["n"] = 1 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode2 {
    Idle,
    Run,
    Stop = 10
}

class Rng2 {
    int state = 0;
    Rng2(int seed) { state = seed + 2; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt2 {
    int x = 0;
    int y = 0;
    Pt2(int px, int py) { x = px; y = py; }
    int manhattan(Pt2 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt2 other) { return x * other.x + y * other.y; }
}

class Worker2 {
    int total = 0;
    int count = 0;
    Mode2 mode = Mode2.Idle;
    string name = "worker2";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode2.Stop; } else { mode = Mode2.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner2 {
    int run() {
    auto rng = new Rng2(2);
    auto worker = new Worker2();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt2(1, 2);
    auto b = new Pt2(4, -6);
    int bias = 2;
    auto scale = n => n * bias;
    table t = { name = "m2", ["n"] = 2 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode3 {
    Idle,
    Run,
    Stop = 10
}

class Rng3 {
    int state = 0;
    Rng3(int seed) { state = seed + 3; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt3 {
    int x = 0;
    int y = 0;
    Pt3(int px, int py) { x = px; y = py; }
    int manhattan(Pt3 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt3 other) { return x * other.x + y * other.y; }
}

class Worker3 {
    int total = 0;
    int count = 0;
    Mode3 mode = Mode3.Idle;
    string name = "worker3";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode3.Stop; } else { mode = Mode3.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner3 {
    int run() {
    auto rng = new Rng3(3);
    auto worker = new Worker3();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt3(1, 2);
    auto b = new Pt3(4, -6);
    int bias = 3;
    auto scale = n => n * bias;
    table t = { name = "m3", ["n"] = 3 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode4 {
    Idle,
    Run,
    Stop = 10
}

class Rng4 {
    int state = 0;
    Rng4(int seed) { state = seed + 4; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt4 {
    int x = 0;
    int y = 0;
    Pt4(int px, int py) { x = px; y = py; }
    int manhattan(Pt4 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt4 other) { return x * other.x + y * other.y; }
}

class Worker4 {
    int total = 0;
    int count = 0;
    Mode4 mode = Mode4.Idle;
    string name = "worker4";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode4.Stop; } else { mode = Mode4.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner4 {
    int run() {
    auto rng = new Rng4(4);
    auto worker = new Worker4();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt4(1, 2);
    auto b = new Pt4(4, -6);
    int bias = 4;
    auto scale = n => n * bias;
    table t = { name = "m4", ["n"] = 4 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode5 {
    Idle,
    Run,
    Stop = 10
}

class Rng5 {
    int state = 0;
    Rng5(int seed) { state = seed + 5; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt5 {
    int x = 0;
    int y = 0;
    Pt5(int px, int py) { x = px; y = py; }
    int manhattan(Pt5 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt5 other) { return x * other.x + y * other.y; }
}

class Worker5 {
    int total = 0;
    int count = 0;
    Mode5 mode = Mode5.Idle;
    string name = "worker5";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode5.Stop; } else { mode = Mode5.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner5 {
    int run() {
    auto rng = new Rng5(5);
    auto worker = new Worker5();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt5(1, 2);
    auto b = new Pt5(4, -6);
    int bias = 5;
    auto scale = n => n * bias;
    table t = { name = "m5", ["n"] = 5 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode6 {
    Idle,
    Run,
    Stop = 10
}

class Rng6 {
    int state = 0;
    Rng6(int seed) { state = seed + 6; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt6 {
    int x = 0;
    int y = 0;
    Pt6(int px, int py) { x = px; y = py; }
    int manhattan(Pt6 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt6 other) { return x * other.x + y * other.y; }
}

class Worker6 {
    int total = 0;
    int count = 0;
    Mode6 mode = Mode6.Idle;
    string name = "worker6";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode6.Stop; } else { mode = Mode6.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner6 {
    int run() {
    auto rng = new Rng6(6);
    auto worker = new Worker6();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt6(1, 2);
    auto b = new Pt6(4, -6);
    int bias = 6;
    auto scale = n => n * bias;
    table t = { name = "m6", ["n"] = 6 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode7 {
    Idle,
    Run,
    Stop = 10
}

class Rng7 {
    int state = 0;
    Rng7(int seed) { state = seed + 7; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt7 {
    int x = 0;
    int y = 0;
    Pt7(int px, int py) { x = px; y = py; }
    int manhattan(Pt7 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt7 other) { return x * other.x + y * other.y; }
}

class Worker7 {
    int total = 0;
    int count = 0;
    Mode7 mode = Mode7.Idle;
    string name = "worker7";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode7.Stop; } else { mode = Mode7.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner7 {
    int run() {
    auto rng = new Rng7(7);
    auto worker = new Worker7();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt7(1, 2);
    auto b = new Pt7(4, -6);
    int bias = 7;
    auto scale = n => n * bias;
    table t = { name = "m7", ["n"] = 7 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode8 {
    Idle,
    Run,
    Stop = 10
}

class Rng8 {
    int state = 0;
    Rng8(int seed) { state = seed + 8; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt8 {
    int x = 0;
    int y = 0;
    Pt8(int px, int py) { x = px; y = py; }
    int manhattan(Pt8 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt8 other) { return x * other.x + y * other.y; }
}

class Worker8 {
    int total = 0;
    int count = 0;
    Mode8 mode = Mode8.Idle;
    string name = "worker8";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode8.Stop; } else { mode = Mode8.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner8 {
    int run() {
    auto rng = new Rng8(8);
    auto worker = new Worker8();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt8(1, 2);
    auto b = new Pt8(4, -6);
    int bias = 8;
    auto scale = n => n * bias;
    table t = { name = "m8", ["n"] = 8 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode9 {
    Idle,
    Run,
    Stop = 10
}

class Rng9 {
    int state = 0;
    Rng9(int seed) { state = seed + 9; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt9 {
    int x = 0;
    int y = 0;
    Pt9(int px, int py) { x = px; y = py; }
    int manhattan(Pt9 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt9 other) { return x * other.x + y * other.y; }
}

class Worker9 {
    int total = 0;
    int count = 0;
    Mode9 mode = Mode9.Idle;
    string name = "worker9";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode9.Stop; } else { mode = Mode9.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner9 {
    int run() {
    auto rng = new Rng9(9);
    auto worker = new Worker9();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt9(1, 2);
    auto b = new Pt9(4, -6);
    int bias = 9;
    auto scale = n => n * bias;
    table t = { name = "m9", ["n"] = 9 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode10 {
    Idle,
    Run,
    Stop = 10
}

class Rng10 {
    int state = 0;
    Rng10(int seed) { state = seed + 10; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt10 {
    int x = 0;
    int y = 0;
    Pt10(int px, int py) { x = px; y = py; }
    int manhattan(Pt10 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt10 other) { return x * other.x + y * other.y; }
}

class Worker10 {
    int total = 0;
    int count = 0;
    Mode10 mode = Mode10.Idle;
    string name = "worker10";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode10.Stop; } else { mode = Mode10.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner10 {
    int run() {
    auto rng = new Rng10(10);
    auto worker = new Worker10();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt10(1, 2);
    auto b = new Pt10(4, -6);
    int bias = 10;
    auto scale = n => n * bias;
    table t = { name = "m10", ["n"] = 10 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode11 {
    Idle,
    Run,
    Stop = 10
}

class Rng11 {
    int state = 0;
    Rng11(int seed) { state = seed + 11; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt11 {
    int x = 0;
    int y = 0;
    Pt11(int px, int py) { x = px; y = py; }
    int manhattan(Pt11 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt11 other) { return x * other.x + y * other.y; }
}

class Worker11 {
    int total = 0;
    int count = 0;
    Mode11 mode = Mode11.Idle;
    string name = "worker11";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode11.Stop; } else { mode = Mode11.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner11 {
    int run() {
    auto rng = new Rng11(11);
    auto worker = new Worker11();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt11(1, 2);
    auto b = new Pt11(4, -6);
    int bias = 11;
    auto scale = n => n * bias;
    table t = { name = "m11", ["n"] = 11 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode12 {
    Idle,
    Run,
    Stop = 10
}

class Rng12 {
    int state = 0;
    Rng12(int seed) { state = seed + 12; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt12 {
    int x = 0;
    int y = 0;
    Pt12(int px, int py) { x = px; y = py; }
    int manhattan(Pt12 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt12 other) { return x * other.x + y * other.y; }
}

class Worker12 {
    int total = 0;
    int count = 0;
    Mode12 mode = Mode12.Idle;
    string name = "worker12";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode12.Stop; } else { mode = Mode12.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner12 {
    int run() {
    auto rng = new Rng12(12);
    auto worker = new Worker12();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt12(1, 2);
    auto b = new Pt12(4, -6);
    int bias = 12;
    auto scale = n => n * bias;
    table t = { name = "m12", ["n"] = 12 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode13 {
    Idle,
    Run,
    Stop = 10
}

class Rng13 {
    int state = 0;
    Rng13(int seed) { state = seed + 13; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt13 {
    int x = 0;
    int y = 0;
    Pt13(int px, int py) { x = px; y = py; }
    int manhattan(Pt13 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt13 other) { return x * other.x + y * other.y; }
}

class Worker13 {
    int total = 0;
    int count = 0;
    Mode13 mode = Mode13.Idle;
    string name = "worker13";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode13.Stop; } else { mode = Mode13.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner13 {
    int run() {
    auto rng = new Rng13(13);
    auto worker = new Worker13();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt13(1, 2);
    auto b = new Pt13(4, -6);
    int bias = 13;
    auto scale = n => n * bias;
    table t = { name = "m13", ["n"] = 13 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode14 {
    Idle,
    Run,
    Stop = 10
}

class Rng14 {
    int state = 0;
    Rng14(int seed) { state = seed + 14; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt14 {
    int x = 0;
    int y = 0;
    Pt14(int px, int py) { x = px; y = py; }
    int manhattan(Pt14 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt14 other) { return x * other.x + y * other.y; }
}

class Worker14 {
    int total = 0;
    int count = 0;
    Mode14 mode = Mode14.Idle;
    string name = "worker14";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode14.Stop; } else { mode = Mode14.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner14 {
    int run() {
    auto rng = new Rng14(14);
    auto worker = new Worker14();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt14(1, 2);
    auto b = new Pt14(4, -6);
    int bias = 14;
    auto scale = n => n * bias;
    table t = { name = "m14", ["n"] = 14 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode15 {
    Idle,
    Run,
    Stop = 10
}

class Rng15 {
    int state = 0;
    Rng15(int seed) { state = seed + 15; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt15 {
    int x = 0;
    int y = 0;
    Pt15(int px, int py) { x = px; y = py; }
    int manhattan(Pt15 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt15 other) { return x * other.x + y * other.y; }
}

class Worker15 {
    int total = 0;
    int count = 0;
    Mode15 mode = Mode15.Idle;
    string name = "worker15";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode15.Stop; } else { mode = Mode15.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner15 {
    int run() {
    auto rng = new Rng15(15);
    auto worker = new Worker15();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt15(1, 2);
    auto b = new Pt15(4, -6);
    int bias = 15;
    auto scale = n => n * bias;
    table t = { name = "m15", ["n"] = 15 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode16 {
    Idle,
    Run,
    Stop = 10
}

class Rng16 {
    int state = 0;
    Rng16(int seed) { state = seed + 16; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt16 {
    int x = 0;
    int y = 0;
    Pt16(int px, int py) { x = px; y = py; }
    int manhattan(Pt16 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt16 other) { return x * other.x + y * other.y; }
}

class Worker16 {
    int total = 0;
    int count = 0;
    Mode16 mode = Mode16.Idle;
    string name = "worker16";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode16.Stop; } else { mode = Mode16.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner16 {
    int run() {
    auto rng = new Rng16(16);
    auto worker = new Worker16();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt16(1, 2);
    auto b = new Pt16(4, -6);
    int bias = 16;
    auto scale = n => n * bias;
    table t = { name = "m16", ["n"] = 16 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode17 {
    Idle,
    Run,
    Stop = 10
}

class Rng17 {
    int state = 0;
    Rng17(int seed) { state = seed + 17; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt17 {
    int x = 0;
    int y = 0;
    Pt17(int px, int py) { x = px; y = py; }
    int manhattan(Pt17 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt17 other) { return x * other.x + y * other.y; }
}

class Worker17 {
    int total = 0;
    int count = 0;
    Mode17 mode = Mode17.Idle;
    string name = "worker17";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode17.Stop; } else { mode = Mode17.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner17 {
    int run() {
    auto rng = new Rng17(17);
    auto worker = new Worker17();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt17(1, 2);
    auto b = new Pt17(4, -6);
    int bias = 17;
    auto scale = n => n * bias;
    table t = { name = "m17", ["n"] = 17 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode18 {
    Idle,
    Run,
    Stop = 10
}

class Rng18 {
    int state = 0;
    Rng18(int seed) { state = seed + 18; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt18 {
    int x = 0;
    int y = 0;
    Pt18(int px, int py) { x = px; y = py; }
    int manhattan(Pt18 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt18 other) { return x * other.x + y * other.y; }
}

class Worker18 {
    int total = 0;
    int count = 0;
    Mode18 mode = Mode18.Idle;
    string name = "worker18";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode18.Stop; } else { mode = Mode18.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner18 {
    int run() {
    auto rng = new Rng18(18);
    auto worker = new Worker18();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt18(1, 2);
    auto b = new Pt18(4, -6);
    int bias = 18;
    auto scale = n => n * bias;
    table t = { name = "m18", ["n"] = 18 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode19 {
    Idle,
    Run,
    Stop = 10
}

class Rng19 {
    int state = 0;
    Rng19(int seed) { state = seed + 19; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt19 {
    int x = 0;
    int y = 0;
    Pt19(int px, int py) { x = px; y = py; }
    int manhattan(Pt19 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt19 other) { return x * other.x + y * other.y; }
}

class Worker19 {
    int total = 0;
    int count = 0;
    Mode19 mode = Mode19.Idle;
    string name = "worker19";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode19.Stop; } else { mode = Mode19.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner19 {
    int run() {
    auto rng = new Rng19(19);
    auto worker = new Worker19();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt19(1, 2);
    auto b = new Pt19(4, -6);
    int bias = 19;
    auto scale = n => n * bias;
    table t = { name = "m19", ["n"] = 19 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode20 {
    Idle,
    Run,
    Stop = 10
}

class Rng20 {
    int state = 0;
    Rng20(int seed) { state = seed + 20; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt20 {
    int x = 0;
    int y = 0;
    Pt20(int px, int py) { x = px; y = py; }
    int manhattan(Pt20 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt20 other) { return x * other.x + y * other.y; }
}

class Worker20 {
    int total = 0;
    int count = 0;
    Mode20 mode = Mode20.Idle;
    string name = "worker20";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode20.Stop; } else { mode = Mode20.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner20 {
    int run() {
    auto rng = new Rng20(20);
    auto worker = new Worker20();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt20(1, 2);
    auto b = new Pt20(4, -6);
    int bias = 20;
    auto scale = n => n * bias;
    table t = { name = "m20", ["n"] = 20 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode21 {
    Idle,
    Run,
    Stop = 10
}

class Rng21 {
    int state = 0;
    Rng21(int seed) { state = seed + 21; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt21 {
    int x = 0;
    int y = 0;
    Pt21(int px, int py) { x = px; y = py; }
    int manhattan(Pt21 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt21 other) { return x * other.x + y * other.y; }
}

class Worker21 {
    int total = 0;
    int count = 0;
    Mode21 mode = Mode21.Idle;
    string name = "worker21";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode21.Stop; } else { mode = Mode21.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner21 {
    int run() {
    auto rng = new Rng21(21);
    auto worker = new Worker21();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt21(1, 2);
    auto b = new Pt21(4, -6);
    int bias = 21;
    auto scale = n => n * bias;
    table t = { name = "m21", ["n"] = 21 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode22 {
    Idle,
    Run,
    Stop = 10
}

class Rng22 {
    int state = 0;
    Rng22(int seed) { state = seed + 22; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt22 {
    int x = 0;
    int y = 0;
    Pt22(int px, int py) { x = px; y = py; }
    int manhattan(Pt22 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt22 other) { return x * other.x + y * other.y; }
}

class Worker22 {
    int total = 0;
    int count = 0;
    Mode22 mode = Mode22.Idle;
    string name = "worker22";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode22.Stop; } else { mode = Mode22.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner22 {
    int run() {
    auto rng = new Rng22(22);
    auto worker = new Worker22();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt22(1, 2);
    auto b = new Pt22(4, -6);
    int bias = 22;
    auto scale = n => n * bias;
    table t = { name = "m22", ["n"] = 22 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode23 {
    Idle,
    Run,
    Stop = 10
}

class Rng23 {
    int state = 0;
    Rng23(int seed) { state = seed + 23; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt23 {
    int x = 0;
    int y = 0;
    Pt23(int px, int py) { x = px; y = py; }
    int manhattan(Pt23 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt23 other) { return x * other.x + y * other.y; }
}

class Worker23 {
    int total = 0;
    int count = 0;
    Mode23 mode = Mode23.Idle;
    string name = "worker23";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode23.Stop; } else { mode = Mode23.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner23 {
    int run() {
    auto rng = new Rng23(23);
    auto worker = new Worker23();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt23(1, 2);
    auto b = new Pt23(4, -6);
    int bias = 23;
    auto scale = n => n * bias;
    table t = { name = "m23", ["n"] = 23 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode24 {
    Idle,
    Run,
    Stop = 10
}

class Rng24 {
    int state = 0;
    Rng24(int seed) { state = seed + 24; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt24 {
    int x = 0;
    int y = 0;
    Pt24(int px, int py) { x = px; y = py; }
    int manhattan(Pt24 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt24 other) { return x * other.x + y * other.y; }
}

class Worker24 {
    int total = 0;
    int count = 0;
    Mode24 mode = Mode24.Idle;
    string name = "worker24";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode24.Stop; } else { mode = Mode24.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner24 {
    int run() {
    auto rng = new Rng24(24);
    auto worker = new Worker24();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt24(1, 2);
    auto b = new Pt24(4, -6);
    int bias = 24;
    auto scale = n => n * bias;
    table t = { name = "m24", ["n"] = 24 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode25 {
    Idle,
    Run,
    Stop = 10
}

class Rng25 {
    int state = 0;
    Rng25(int seed) { state = seed + 25; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt25 {
    int x = 0;
    int y = 0;
    Pt25(int px, int py) { x = px; y = py; }
    int manhattan(Pt25 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt25 other) { return x * other.x + y * other.y; }
}

class Worker25 {
    int total = 0;
    int count = 0;
    Mode25 mode = Mode25.Idle;
    string name = "worker25";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode25.Stop; } else { mode = Mode25.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner25 {
    int run() {
    auto rng = new Rng25(25);
    auto worker = new Worker25();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt25(1, 2);
    auto b = new Pt25(4, -6);
    int bias = 25;
    auto scale = n => n * bias;
    table t = { name = "m25", ["n"] = 25 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode26 {
    Idle,
    Run,
    Stop = 10
}

class Rng26 {
    int state = 0;
    Rng26(int seed) { state = seed + 26; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt26 {
    int x = 0;
    int y = 0;
    Pt26(int px, int py) { x = px; y = py; }
    int manhattan(Pt26 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt26 other) { return x * other.x + y * other.y; }
}

class Worker26 {
    int total = 0;
    int count = 0;
    Mode26 mode = Mode26.Idle;
    string name = "worker26";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode26.Stop; } else { mode = Mode26.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner26 {
    int run() {
    auto rng = new Rng26(26);
    auto worker = new Worker26();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt26(1, 2);
    auto b = new Pt26(4, -6);
    int bias = 26;
    auto scale = n => n * bias;
    table t = { name = "m26", ["n"] = 26 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode27 {
    Idle,
    Run,
    Stop = 10
}

class Rng27 {
    int state = 0;
    Rng27(int seed) { state = seed + 27; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt27 {
    int x = 0;
    int y = 0;
    Pt27(int px, int py) { x = px; y = py; }
    int manhattan(Pt27 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt27 other) { return x * other.x + y * other.y; }
}

class Worker27 {
    int total = 0;
    int count = 0;
    Mode27 mode = Mode27.Idle;
    string name = "worker27";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode27.Stop; } else { mode = Mode27.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner27 {
    int run() {
    auto rng = new Rng27(27);
    auto worker = new Worker27();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt27(1, 2);
    auto b = new Pt27(4, -6);
    int bias = 27;
    auto scale = n => n * bias;
    table t = { name = "m27", ["n"] = 27 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode28 {
    Idle,
    Run,
    Stop = 10
}

class Rng28 {
    int state = 0;
    Rng28(int seed) { state = seed + 28; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt28 {
    int x = 0;
    int y = 0;
    Pt28(int px, int py) { x = px; y = py; }
    int manhattan(Pt28 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt28 other) { return x * other.x + y * other.y; }
}

class Worker28 {
    int total = 0;
    int count = 0;
    Mode28 mode = Mode28.Idle;
    string name = "worker28";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode28.Stop; } else { mode = Mode28.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner28 {
    int run() {
    auto rng = new Rng28(28);
    auto worker = new Worker28();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt28(1, 2);
    auto b = new Pt28(4, -6);
    int bias = 28;
    auto scale = n => n * bias;
    table t = { name = "m28", ["n"] = 28 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode29 {
    Idle,
    Run,
    Stop = 10
}

class Rng29 {
    int state = 0;
    Rng29(int seed) { state = seed + 29; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt29 {
    int x = 0;
    int y = 0;
    Pt29(int px, int py) { x = px; y = py; }
    int manhattan(Pt29 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt29 other) { return x * other.x + y * other.y; }
}

class Worker29 {
    int total = 0;
    int count = 0;
    Mode29 mode = Mode29.Idle;
    string name = "worker29";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode29.Stop; } else { mode = Mode29.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner29 {
    int run() {
    auto rng = new Rng29(29);
    auto worker = new Worker29();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt29(1, 2);
    auto b = new Pt29(4, -6);
    int bias = 29;
    auto scale = n => n * bias;
    table t = { name = "m29", ["n"] = 29 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode30 {
    Idle,
    Run,
    Stop = 10
}

class Rng30 {
    int state = 0;
    Rng30(int seed) { state = seed + 30; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt30 {
    int x = 0;
    int y = 0;
    Pt30(int px, int py) { x = px; y = py; }
    int manhattan(Pt30 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt30 other) { return x * other.x + y * other.y; }
}

class Worker30 {
    int total = 0;
    int count = 0;
    Mode30 mode = Mode30.Idle;
    string name = "worker30";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode30.Stop; } else { mode = Mode30.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner30 {
    int run() {
    auto rng = new Rng30(30);
    auto worker = new Worker30();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt30(1, 2);
    auto b = new Pt30(4, -6);
    int bias = 30;
    auto scale = n => n * bias;
    table t = { name = "m30", ["n"] = 30 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode31 {
    Idle,
    Run,
    Stop = 10
}

class Rng31 {
    int state = 0;
    Rng31(int seed) { state = seed + 31; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt31 {
    int x = 0;
    int y = 0;
    Pt31(int px, int py) { x = px; y = py; }
    int manhattan(Pt31 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt31 other) { return x * other.x + y * other.y; }
}

class Worker31 {
    int total = 0;
    int count = 0;
    Mode31 mode = Mode31.Idle;
    string name = "worker31";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode31.Stop; } else { mode = Mode31.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner31 {
    int run() {
    auto rng = new Rng31(31);
    auto worker = new Worker31();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt31(1, 2);
    auto b = new Pt31(4, -6);
    int bias = 31;
    auto scale = n => n * bias;
    table t = { name = "m31", ["n"] = 31 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode32 {
    Idle,
    Run,
    Stop = 10
}

class Rng32 {
    int state = 0;
    Rng32(int seed) { state = seed + 32; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt32 {
    int x = 0;
    int y = 0;
    Pt32(int px, int py) { x = px; y = py; }
    int manhattan(Pt32 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt32 other) { return x * other.x + y * other.y; }
}

class Worker32 {
    int total = 0;
    int count = 0;
    Mode32 mode = Mode32.Idle;
    string name = "worker32";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode32.Stop; } else { mode = Mode32.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner32 {
    int run() {
    auto rng = new Rng32(32);
    auto worker = new Worker32();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt32(1, 2);
    auto b = new Pt32(4, -6);
    int bias = 32;
    auto scale = n => n * bias;
    table t = { name = "m32", ["n"] = 32 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode33 {
    Idle,
    Run,
    Stop = 10
}

class Rng33 {
    int state = 0;
    Rng33(int seed) { state = seed + 33; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt33 {
    int x = 0;
    int y = 0;
    Pt33(int px, int py) { x = px; y = py; }
    int manhattan(Pt33 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt33 other) { return x * other.x + y * other.y; }
}

class Worker33 {
    int total = 0;
    int count = 0;
    Mode33 mode = Mode33.Idle;
    string name = "worker33";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode33.Stop; } else { mode = Mode33.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner33 {
    int run() {
    auto rng = new Rng33(33);
    auto worker = new Worker33();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt33(1, 2);
    auto b = new Pt33(4, -6);
    int bias = 33;
    auto scale = n => n * bias;
    table t = { name = "m33", ["n"] = 33 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode34 {
    Idle,
    Run,
    Stop = 10
}

class Rng34 {
    int state = 0;
    Rng34(int seed) { state = seed + 34; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt34 {
    int x = 0;
    int y = 0;
    Pt34(int px, int py) { x = px; y = py; }
    int manhattan(Pt34 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt34 other) { return x * other.x + y * other.y; }
}

class Worker34 {
    int total = 0;
    int count = 0;
    Mode34 mode = Mode34.Idle;
    string name = "worker34";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode34.Stop; } else { mode = Mode34.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner34 {
    int run() {
    auto rng = new Rng34(34);
    auto worker = new Worker34();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt34(1, 2);
    auto b = new Pt34(4, -6);
    int bias = 34;
    auto scale = n => n * bias;
    table t = { name = "m34", ["n"] = 34 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode35 {
    Idle,
    Run,
    Stop = 10
}

class Rng35 {
    int state = 0;
    Rng35(int seed) { state = seed + 35; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt35 {
    int x = 0;
    int y = 0;
    Pt35(int px, int py) { x = px; y = py; }
    int manhattan(Pt35 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt35 other) { return x * other.x + y * other.y; }
}

class Worker35 {
    int total = 0;
    int count = 0;
    Mode35 mode = Mode35.Idle;
    string name = "worker35";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode35.Stop; } else { mode = Mode35.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner35 {
    int run() {
    auto rng = new Rng35(35);
    auto worker = new Worker35();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt35(1, 2);
    auto b = new Pt35(4, -6);
    int bias = 35;
    auto scale = n => n * bias;
    table t = { name = "m35", ["n"] = 35 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode36 {
    Idle,
    Run,
    Stop = 10
}

class Rng36 {
    int state = 0;
    Rng36(int seed) { state = seed + 36; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt36 {
    int x = 0;
    int y = 0;
    Pt36(int px, int py) { x = px; y = py; }
    int manhattan(Pt36 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt36 other) { return x * other.x + y * other.y; }
}

class Worker36 {
    int total = 0;
    int count = 0;
    Mode36 mode = Mode36.Idle;
    string name = "worker36";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode36.Stop; } else { mode = Mode36.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner36 {
    int run() {
    auto rng = new Rng36(36);
    auto worker = new Worker36();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt36(1, 2);
    auto b = new Pt36(4, -6);
    int bias = 36;
    auto scale = n => n * bias;
    table t = { name = "m36", ["n"] = 36 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode37 {
    Idle,
    Run,
    Stop = 10
}

class Rng37 {
    int state = 0;
    Rng37(int seed) { state = seed + 37; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt37 {
    int x = 0;
    int y = 0;
    Pt37(int px, int py) { x = px; y = py; }
    int manhattan(Pt37 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt37 other) { return x * other.x + y * other.y; }
}

class Worker37 {
    int total = 0;
    int count = 0;
    Mode37 mode = Mode37.Idle;
    string name = "worker37";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode37.Stop; } else { mode = Mode37.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner37 {
    int run() {
    auto rng = new Rng37(37);
    auto worker = new Worker37();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt37(1, 2);
    auto b = new Pt37(4, -6);
    int bias = 37;
    auto scale = n => n * bias;
    table t = { name = "m37", ["n"] = 37 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode38 {
    Idle,
    Run,
    Stop = 10
}

class Rng38 {
    int state = 0;
    Rng38(int seed) { state = seed + 38; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt38 {
    int x = 0;
    int y = 0;
    Pt38(int px, int py) { x = px; y = py; }
    int manhattan(Pt38 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt38 other) { return x * other.x + y * other.y; }
}

class Worker38 {
    int total = 0;
    int count = 0;
    Mode38 mode = Mode38.Idle;
    string name = "worker38";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode38.Stop; } else { mode = Mode38.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner38 {
    int run() {
    auto rng = new Rng38(38);
    auto worker = new Worker38();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt38(1, 2);
    auto b = new Pt38(4, -6);
    int bias = 38;
    auto scale = n => n * bias;
    table t = { name = "m38", ["n"] = 38 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode39 {
    Idle,
    Run,
    Stop = 10
}

class Rng39 {
    int state = 0;
    Rng39(int seed) { state = seed + 39; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt39 {
    int x = 0;
    int y = 0;
    Pt39(int px, int py) { x = px; y = py; }
    int manhattan(Pt39 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt39 other) { return x * other.x + y * other.y; }
}

class Worker39 {
    int total = 0;
    int count = 0;
    Mode39 mode = Mode39.Idle;
    string name = "worker39";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode39.Stop; } else { mode = Mode39.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner39 {
    int run() {
    auto rng = new Rng39(39);
    auto worker = new Worker39();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt39(1, 2);
    auto b = new Pt39(4, -6);
    int bias = 39;
    auto scale = n => n * bias;
    table t = { name = "m39", ["n"] = 39 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode40 {
    Idle,
    Run,
    Stop = 10
}

class Rng40 {
    int state = 0;
    Rng40(int seed) { state = seed + 40; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt40 {
    int x = 0;
    int y = 0;
    Pt40(int px, int py) { x = px; y = py; }
    int manhattan(Pt40 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt40 other) { return x * other.x + y * other.y; }
}

class Worker40 {
    int total = 0;
    int count = 0;
    Mode40 mode = Mode40.Idle;
    string name = "worker40";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode40.Stop; } else { mode = Mode40.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner40 {
    int run() {
    auto rng = new Rng40(40);
    auto worker = new Worker40();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt40(1, 2);
    auto b = new Pt40(4, -6);
    int bias = 40;
    auto scale = n => n * bias;
    table t = { name = "m40", ["n"] = 40 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode41 {
    Idle,
    Run,
    Stop = 10
}

class Rng41 {
    int state = 0;
    Rng41(int seed) { state = seed + 41; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt41 {
    int x = 0;
    int y = 0;
    Pt41(int px, int py) { x = px; y = py; }
    int manhattan(Pt41 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt41 other) { return x * other.x + y * other.y; }
}

class Worker41 {
    int total = 0;
    int count = 0;
    Mode41 mode = Mode41.Idle;
    string name = "worker41";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode41.Stop; } else { mode = Mode41.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner41 {
    int run() {
    auto rng = new Rng41(41);
    auto worker = new Worker41();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt41(1, 2);
    auto b = new Pt41(4, -6);
    int bias = 41;
    auto scale = n => n * bias;
    table t = { name = "m41", ["n"] = 41 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode42 {
    Idle,
    Run,
    Stop = 10
}

class Rng42 {
    int state = 0;
    Rng42(int seed) { state = seed + 42; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt42 {
    int x = 0;
    int y = 0;
    Pt42(int px, int py) { x = px; y = py; }
    int manhattan(Pt42 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt42 other) { return x * other.x + y * other.y; }
}

class Worker42 {
    int total = 0;
    int count = 0;
    Mode42 mode = Mode42.Idle;
    string name = "worker42";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode42.Stop; } else { mode = Mode42.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner42 {
    int run() {
    auto rng = new Rng42(42);
    auto worker = new Worker42();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt42(1, 2);
    auto b = new Pt42(4, -6);
    int bias = 42;
    auto scale = n => n * bias;
    table t = { name = "m42", ["n"] = 42 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode43 {
    Idle,
    Run,
    Stop = 10
}

class Rng43 {
    int state = 0;
    Rng43(int seed) { state = seed + 43; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt43 {
    int x = 0;
    int y = 0;
    Pt43(int px, int py) { x = px; y = py; }
    int manhattan(Pt43 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt43 other) { return x * other.x + y * other.y; }
}

class Worker43 {
    int total = 0;
    int count = 0;
    Mode43 mode = Mode43.Idle;
    string name = "worker43";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode43.Stop; } else { mode = Mode43.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner43 {
    int run() {
    auto rng = new Rng43(43);
    auto worker = new Worker43();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt43(1, 2);
    auto b = new Pt43(4, -6);
    int bias = 43;
    auto scale = n => n * bias;
    table t = { name = "m43", ["n"] = 43 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode44 {
    Idle,
    Run,
    Stop = 10
}

class Rng44 {
    int state = 0;
    Rng44(int seed) { state = seed + 44; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt44 {
    int x = 0;
    int y = 0;
    Pt44(int px, int py) { x = px; y = py; }
    int manhattan(Pt44 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt44 other) { return x * other.x + y * other.y; }
}

class Worker44 {
    int total = 0;
    int count = 0;
    Mode44 mode = Mode44.Idle;
    string name = "worker44";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode44.Stop; } else { mode = Mode44.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner44 {
    int run() {
    auto rng = new Rng44(44);
    auto worker = new Worker44();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt44(1, 2);
    auto b = new Pt44(4, -6);
    int bias = 44;
    auto scale = n => n * bias;
    table t = { name = "m44", ["n"] = 44 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode45 {
    Idle,
    Run,
    Stop = 10
}

class Rng45 {
    int state = 0;
    Rng45(int seed) { state = seed + 45; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt45 {
    int x = 0;
    int y = 0;
    Pt45(int px, int py) { x = px; y = py; }
    int manhattan(Pt45 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt45 other) { return x * other.x + y * other.y; }
}

class Worker45 {
    int total = 0;
    int count = 0;
    Mode45 mode = Mode45.Idle;
    string name = "worker45";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode45.Stop; } else { mode = Mode45.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner45 {
    int run() {
    auto rng = new Rng45(45);
    auto worker = new Worker45();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt45(1, 2);
    auto b = new Pt45(4, -6);
    int bias = 45;
    auto scale = n => n * bias;
    table t = { name = "m45", ["n"] = 45 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode46 {
    Idle,
    Run,
    Stop = 10
}

class Rng46 {
    int state = 0;
    Rng46(int seed) { state = seed + 46; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt46 {
    int x = 0;
    int y = 0;
    Pt46(int px, int py) { x = px; y = py; }
    int manhattan(Pt46 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt46 other) { return x * other.x + y * other.y; }
}

class Worker46 {
    int total = 0;
    int count = 0;
    Mode46 mode = Mode46.Idle;
    string name = "worker46";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode46.Stop; } else { mode = Mode46.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner46 {
    int run() {
    auto rng = new Rng46(46);
    auto worker = new Worker46();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt46(1, 2);
    auto b = new Pt46(4, -6);
    int bias = 46;
    auto scale = n => n * bias;
    table t = { name = "m46", ["n"] = 46 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode47 {
    Idle,
    Run,
    Stop = 10
}

class Rng47 {
    int state = 0;
    Rng47(int seed) { state = seed + 47; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt47 {
    int x = 0;
    int y = 0;
    Pt47(int px, int py) { x = px; y = py; }
    int manhattan(Pt47 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt47 other) { return x * other.x + y * other.y; }
}

class Worker47 {
    int total = 0;
    int count = 0;
    Mode47 mode = Mode47.Idle;
    string name = "worker47";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode47.Stop; } else { mode = Mode47.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner47 {
    int run() {
    auto rng = new Rng47(47);
    auto worker = new Worker47();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt47(1, 2);
    auto b = new Pt47(4, -6);
    int bias = 47;
    auto scale = n => n * bias;
    table t = { name = "m47", ["n"] = 47 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode48 {
    Idle,
    Run,
    Stop = 10
}

class Rng48 {
    int state = 0;
    Rng48(int seed) { state = seed + 48; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt48 {
    int x = 0;
    int y = 0;
    Pt48(int px, int py) { x = px; y = py; }
    int manhattan(Pt48 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt48 other) { return x * other.x + y * other.y; }
}

class Worker48 {
    int total = 0;
    int count = 0;
    Mode48 mode = Mode48.Idle;
    string name = "worker48";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode48.Stop; } else { mode = Mode48.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner48 {
    int run() {
    auto rng = new Rng48(48);
    auto worker = new Worker48();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt48(1, 2);
    auto b = new Pt48(4, -6);
    int bias = 48;
    auto scale = n => n * bias;
    table t = { name = "m48", ["n"] = 48 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode49 {
    Idle,
    Run,
    Stop = 10
}

class Rng49 {
    int state = 0;
    Rng49(int seed) { state = seed + 49; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt49 {
    int x = 0;
    int y = 0;
    Pt49(int px, int py) { x = px; y = py; }
    int manhattan(Pt49 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt49 other) { return x * other.x + y * other.y; }
}

class Worker49 {
    int total = 0;
    int count = 0;
    Mode49 mode = Mode49.Idle;
    string name = "worker49";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode49.Stop; } else { mode = Mode49.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner49 {
    int run() {
    auto rng = new Rng49(49);
    auto worker = new Worker49();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt49(1, 2);
    auto b = new Pt49(4, -6);
    int bias = 49;
    auto scale = n => n * bias;
    table t = { name = "m49", ["n"] = 49 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode50 {
    Idle,
    Run,
    Stop = 10
}

class Rng50 {
    int state = 0;
    Rng50(int seed) { state = seed + 50; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt50 {
    int x = 0;
    int y = 0;
    Pt50(int px, int py) { x = px; y = py; }
    int manhattan(Pt50 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt50 other) { return x * other.x + y * other.y; }
}

class Worker50 {
    int total = 0;
    int count = 0;
    Mode50 mode = Mode50.Idle;
    string name = "worker50";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode50.Stop; } else { mode = Mode50.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner50 {
    int run() {
    auto rng = new Rng50(50);
    auto worker = new Worker50();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt50(1, 2);
    auto b = new Pt50(4, -6);
    int bias = 50;
    auto scale = n => n * bias;
    table t = { name = "m50", ["n"] = 50 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode51 {
    Idle,
    Run,
    Stop = 10
}

class Rng51 {
    int state = 0;
    Rng51(int seed) { state = seed + 51; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt51 {
    int x = 0;
    int y = 0;
    Pt51(int px, int py) { x = px; y = py; }
    int manhattan(Pt51 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt51 other) { return x * other.x + y * other.y; }
}

class Worker51 {
    int total = 0;
    int count = 0;
    Mode51 mode = Mode51.Idle;
    string name = "worker51";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode51.Stop; } else { mode = Mode51.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner51 {
    int run() {
    auto rng = new Rng51(51);
    auto worker = new Worker51();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt51(1, 2);
    auto b = new Pt51(4, -6);
    int bias = 51;
    auto scale = n => n * bias;
    table t = { name = "m51", ["n"] = 51 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode52 {
    Idle,
    Run,
    Stop = 10
}

class Rng52 {
    int state = 0;
    Rng52(int seed) { state = seed + 52; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt52 {
    int x = 0;
    int y = 0;
    Pt52(int px, int py) { x = px; y = py; }
    int manhattan(Pt52 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt52 other) { return x * other.x + y * other.y; }
}

class Worker52 {
    int total = 0;
    int count = 0;
    Mode52 mode = Mode52.Idle;
    string name = "worker52";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode52.Stop; } else { mode = Mode52.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner52 {
    int run() {
    auto rng = new Rng52(52);
    auto worker = new Worker52();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt52(1, 2);
    auto b = new Pt52(4, -6);
    int bias = 52;
    auto scale = n => n * bias;
    table t = { name = "m52", ["n"] = 52 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode53 {
    Idle,
    Run,
    Stop = 10
}

class Rng53 {
    int state = 0;
    Rng53(int seed) { state = seed + 53; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt53 {
    int x = 0;
    int y = 0;
    Pt53(int px, int py) { x = px; y = py; }
    int manhattan(Pt53 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt53 other) { return x * other.x + y * other.y; }
}

class Worker53 {
    int total = 0;
    int count = 0;
    Mode53 mode = Mode53.Idle;
    string name = "worker53";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode53.Stop; } else { mode = Mode53.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner53 {
    int run() {
    auto rng = new Rng53(53);
    auto worker = new Worker53();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt53(1, 2);
    auto b = new Pt53(4, -6);
    int bias = 53;
    auto scale = n => n * bias;
    table t = { name = "m53", ["n"] = 53 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode54 {
    Idle,
    Run,
    Stop = 10
}

class Rng54 {
    int state = 0;
    Rng54(int seed) { state = seed + 54; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt54 {
    int x = 0;
    int y = 0;
    Pt54(int px, int py) { x = px; y = py; }
    int manhattan(Pt54 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt54 other) { return x * other.x + y * other.y; }
}

class Worker54 {
    int total = 0;
    int count = 0;
    Mode54 mode = Mode54.Idle;
    string name = "worker54";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode54.Stop; } else { mode = Mode54.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner54 {
    int run() {
    auto rng = new Rng54(54);
    auto worker = new Worker54();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt54(1, 2);
    auto b = new Pt54(4, -6);
    int bias = 54;
    auto scale = n => n * bias;
    table t = { name = "m54", ["n"] = 54 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode55 {
    Idle,
    Run,
    Stop = 10
}

class Rng55 {
    int state = 0;
    Rng55(int seed) { state = seed + 55; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt55 {
    int x = 0;
    int y = 0;
    Pt55(int px, int py) { x = px; y = py; }
    int manhattan(Pt55 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt55 other) { return x * other.x + y * other.y; }
}

class Worker55 {
    int total = 0;
    int count = 0;
    Mode55 mode = Mode55.Idle;
    string name = "worker55";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode55.Stop; } else { mode = Mode55.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner55 {
    int run() {
    auto rng = new Rng55(55);
    auto worker = new Worker55();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt55(1, 2);
    auto b = new Pt55(4, -6);
    int bias = 55;
    auto scale = n => n * bias;
    table t = { name = "m55", ["n"] = 55 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode56 {
    Idle,
    Run,
    Stop = 10
}

class Rng56 {
    int state = 0;
    Rng56(int seed) { state = seed + 56; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt56 {
    int x = 0;
    int y = 0;
    Pt56(int px, int py) { x = px; y = py; }
    int manhattan(Pt56 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt56 other) { return x * other.x + y * other.y; }
}

class Worker56 {
    int total = 0;
    int count = 0;
    Mode56 mode = Mode56.Idle;
    string name = "worker56";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode56.Stop; } else { mode = Mode56.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner56 {
    int run() {
    auto rng = new Rng56(56);
    auto worker = new Worker56();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt56(1, 2);
    auto b = new Pt56(4, -6);
    int bias = 56;
    auto scale = n => n * bias;
    table t = { name = "m56", ["n"] = 56 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode57 {
    Idle,
    Run,
    Stop = 10
}

class Rng57 {
    int state = 0;
    Rng57(int seed) { state = seed + 57; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt57 {
    int x = 0;
    int y = 0;
    Pt57(int px, int py) { x = px; y = py; }
    int manhattan(Pt57 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt57 other) { return x * other.x + y * other.y; }
}

class Worker57 {
    int total = 0;
    int count = 0;
    Mode57 mode = Mode57.Idle;
    string name = "worker57";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode57.Stop; } else { mode = Mode57.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner57 {
    int run() {
    auto rng = new Rng57(57);
    auto worker = new Worker57();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt57(1, 2);
    auto b = new Pt57(4, -6);
    int bias = 57;
    auto scale = n => n * bias;
    table t = { name = "m57", ["n"] = 57 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode58 {
    Idle,
    Run,
    Stop = 10
}

class Rng58 {
    int state = 0;
    Rng58(int seed) { state = seed + 58; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt58 {
    int x = 0;
    int y = 0;
    Pt58(int px, int py) { x = px; y = py; }
    int manhattan(Pt58 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt58 other) { return x * other.x + y * other.y; }
}

class Worker58 {
    int total = 0;
    int count = 0;
    Mode58 mode = Mode58.Idle;
    string name = "worker58";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode58.Stop; } else { mode = Mode58.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner58 {
    int run() {
    auto rng = new Rng58(58);
    auto worker = new Worker58();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt58(1, 2);
    auto b = new Pt58(4, -6);
    int bias = 58;
    auto scale = n => n * bias;
    table t = { name = "m58", ["n"] = 58 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode59 {
    Idle,
    Run,
    Stop = 10
}

class Rng59 {
    int state = 0;
    Rng59(int seed) { state = seed + 59; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt59 {
    int x = 0;
    int y = 0;
    Pt59(int px, int py) { x = px; y = py; }
    int manhattan(Pt59 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt59 other) { return x * other.x + y * other.y; }
}

class Worker59 {
    int total = 0;
    int count = 0;
    Mode59 mode = Mode59.Idle;
    string name = "worker59";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode59.Stop; } else { mode = Mode59.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner59 {
    int run() {
    auto rng = new Rng59(59);
    auto worker = new Worker59();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt59(1, 2);
    auto b = new Pt59(4, -6);
    int bias = 59;
    auto scale = n => n * bias;
    table t = { name = "m59", ["n"] = 59 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode60 {
    Idle,
    Run,
    Stop = 10
}

class Rng60 {
    int state = 0;
    Rng60(int seed) { state = seed + 60; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt60 {
    int x = 0;
    int y = 0;
    Pt60(int px, int py) { x = px; y = py; }
    int manhattan(Pt60 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt60 other) { return x * other.x + y * other.y; }
}

class Worker60 {
    int total = 0;
    int count = 0;
    Mode60 mode = Mode60.Idle;
    string name = "worker60";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode60.Stop; } else { mode = Mode60.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner60 {
    int run() {
    auto rng = new Rng60(60);
    auto worker = new Worker60();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt60(1, 2);
    auto b = new Pt60(4, -6);
    int bias = 60;
    auto scale = n => n * bias;
    table t = { name = "m60", ["n"] = 60 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode61 {
    Idle,
    Run,
    Stop = 10
}

class Rng61 {
    int state = 0;
    Rng61(int seed) { state = seed + 61; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt61 {
    int x = 0;
    int y = 0;
    Pt61(int px, int py) { x = px; y = py; }
    int manhattan(Pt61 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt61 other) { return x * other.x + y * other.y; }
}

class Worker61 {
    int total = 0;
    int count = 0;
    Mode61 mode = Mode61.Idle;
    string name = "worker61";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode61.Stop; } else { mode = Mode61.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner61 {
    int run() {
    auto rng = new Rng61(61);
    auto worker = new Worker61();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt61(1, 2);
    auto b = new Pt61(4, -6);
    int bias = 61;
    auto scale = n => n * bias;
    table t = { name = "m61", ["n"] = 61 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode62 {
    Idle,
    Run,
    Stop = 10
}

class Rng62 {
    int state = 0;
    Rng62(int seed) { state = seed + 62; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt62 {
    int x = 0;
    int y = 0;
    Pt62(int px, int py) { x = px; y = py; }
    int manhattan(Pt62 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt62 other) { return x * other.x + y * other.y; }
}

class Worker62 {
    int total = 0;
    int count = 0;
    Mode62 mode = Mode62.Idle;
    string name = "worker62";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode62.Stop; } else { mode = Mode62.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner62 {
    int run() {
    auto rng = new Rng62(62);
    auto worker = new Worker62();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt62(1, 2);
    auto b = new Pt62(4, -6);
    int bias = 62;
    auto scale = n => n * bias;
    table t = { name = "m62", ["n"] = 62 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode63 {
    Idle,
    Run,
    Stop = 10
}

class Rng63 {
    int state = 0;
    Rng63(int seed) { state = seed + 63; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt63 {
    int x = 0;
    int y = 0;
    Pt63(int px, int py) { x = px; y = py; }
    int manhattan(Pt63 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt63 other) { return x * other.x + y * other.y; }
}

class Worker63 {
    int total = 0;
    int count = 0;
    Mode63 mode = Mode63.Idle;
    string name = "worker63";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode63.Stop; } else { mode = Mode63.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner63 {
    int run() {
    auto rng = new Rng63(63);
    auto worker = new Worker63();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt63(1, 2);
    auto b = new Pt63(4, -6);
    int bias = 63;
    auto scale = n => n * bias;
    table t = { name = "m63", ["n"] = 63 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode64 {
    Idle,
    Run,
    Stop = 10
}

class Rng64 {
    int state = 0;
    Rng64(int seed) { state = seed + 64; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt64 {
    int x = 0;
    int y = 0;
    Pt64(int px, int py) { x = px; y = py; }
    int manhattan(Pt64 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt64 other) { return x * other.x + y * other.y; }
}

class Worker64 {
    int total = 0;
    int count = 0;
    Mode64 mode = Mode64.Idle;
    string name = "worker64";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode64.Stop; } else { mode = Mode64.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner64 {
    int run() {
    auto rng = new Rng64(64);
    auto worker = new Worker64();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt64(1, 2);
    auto b = new Pt64(4, -6);
    int bias = 64;
    auto scale = n => n * bias;
    table t = { name = "m64", ["n"] = 64 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode65 {
    Idle,
    Run,
    Stop = 10
}

class Rng65 {
    int state = 0;
    Rng65(int seed) { state = seed + 65; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt65 {
    int x = 0;
    int y = 0;
    Pt65(int px, int py) { x = px; y = py; }
    int manhattan(Pt65 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt65 other) { return x * other.x + y * other.y; }
}

class Worker65 {
    int total = 0;
    int count = 0;
    Mode65 mode = Mode65.Idle;
    string name = "worker65";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode65.Stop; } else { mode = Mode65.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner65 {
    int run() {
    auto rng = new Rng65(65);
    auto worker = new Worker65();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt65(1, 2);
    auto b = new Pt65(4, -6);
    int bias = 65;
    auto scale = n => n * bias;
    table t = { name = "m65", ["n"] = 65 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode66 {
    Idle,
    Run,
    Stop = 10
}

class Rng66 {
    int state = 0;
    Rng66(int seed) { state = seed + 66; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt66 {
    int x = 0;
    int y = 0;
    Pt66(int px, int py) { x = px; y = py; }
    int manhattan(Pt66 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt66 other) { return x * other.x + y * other.y; }
}

class Worker66 {
    int total = 0;
    int count = 0;
    Mode66 mode = Mode66.Idle;
    string name = "worker66";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode66.Stop; } else { mode = Mode66.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner66 {
    int run() {
    auto rng = new Rng66(66);
    auto worker = new Worker66();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt66(1, 2);
    auto b = new Pt66(4, -6);
    int bias = 66;
    auto scale = n => n * bias;
    table t = { name = "m66", ["n"] = 66 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode67 {
    Idle,
    Run,
    Stop = 10
}

class Rng67 {
    int state = 0;
    Rng67(int seed) { state = seed + 67; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt67 {
    int x = 0;
    int y = 0;
    Pt67(int px, int py) { x = px; y = py; }
    int manhattan(Pt67 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt67 other) { return x * other.x + y * other.y; }
}

class Worker67 {
    int total = 0;
    int count = 0;
    Mode67 mode = Mode67.Idle;
    string name = "worker67";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode67.Stop; } else { mode = Mode67.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner67 {
    int run() {
    auto rng = new Rng67(67);
    auto worker = new Worker67();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt67(1, 2);
    auto b = new Pt67(4, -6);
    int bias = 67;
    auto scale = n => n * bias;
    table t = { name = "m67", ["n"] = 67 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode68 {
    Idle,
    Run,
    Stop = 10
}

class Rng68 {
    int state = 0;
    Rng68(int seed) { state = seed + 68; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt68 {
    int x = 0;
    int y = 0;
    Pt68(int px, int py) { x = px; y = py; }
    int manhattan(Pt68 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt68 other) { return x * other.x + y * other.y; }
}

class Worker68 {
    int total = 0;
    int count = 0;
    Mode68 mode = Mode68.Idle;
    string name = "worker68";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode68.Stop; } else { mode = Mode68.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner68 {
    int run() {
    auto rng = new Rng68(68);
    auto worker = new Worker68();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt68(1, 2);
    auto b = new Pt68(4, -6);
    int bias = 68;
    auto scale = n => n * bias;
    table t = { name = "m68", ["n"] = 68 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode69 {
    Idle,
    Run,
    Stop = 10
}

class Rng69 {
    int state = 0;
    Rng69(int seed) { state = seed + 69; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt69 {
    int x = 0;
    int y = 0;
    Pt69(int px, int py) { x = px; y = py; }
    int manhattan(Pt69 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt69 other) { return x * other.x + y * other.y; }
}

class Worker69 {
    int total = 0;
    int count = 0;
    Mode69 mode = Mode69.Idle;
    string name = "worker69";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode69.Stop; } else { mode = Mode69.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner69 {
    int run() {
    auto rng = new Rng69(69);
    auto worker = new Worker69();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt69(1, 2);
    auto b = new Pt69(4, -6);
    int bias = 69;
    auto scale = n => n * bias;
    table t = { name = "m69", ["n"] = 69 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode70 {
    Idle,
    Run,
    Stop = 10
}

class Rng70 {
    int state = 0;
    Rng70(int seed) { state = seed + 70; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt70 {
    int x = 0;
    int y = 0;
    Pt70(int px, int py) { x = px; y = py; }
    int manhattan(Pt70 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt70 other) { return x * other.x + y * other.y; }
}

class Worker70 {
    int total = 0;
    int count = 0;
    Mode70 mode = Mode70.Idle;
    string name = "worker70";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode70.Stop; } else { mode = Mode70.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner70 {
    int run() {
    auto rng = new Rng70(70);
    auto worker = new Worker70();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt70(1, 2);
    auto b = new Pt70(4, -6);
    int bias = 70;
    auto scale = n => n * bias;
    table t = { name = "m70", ["n"] = 70 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode71 {
    Idle,
    Run,
    Stop = 10
}

class Rng71 {
    int state = 0;
    Rng71(int seed) { state = seed + 71; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt71 {
    int x = 0;
    int y = 0;
    Pt71(int px, int py) { x = px; y = py; }
    int manhattan(Pt71 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt71 other) { return x * other.x + y * other.y; }
}

class Worker71 {
    int total = 0;
    int count = 0;
    Mode71 mode = Mode71.Idle;
    string name = "worker71";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode71.Stop; } else { mode = Mode71.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner71 {
    int run() {
    auto rng = new Rng71(71);
    auto worker = new Worker71();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt71(1, 2);
    auto b = new Pt71(4, -6);
    int bias = 71;
    auto scale = n => n * bias;
    table t = { name = "m71", ["n"] = 71 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode72 {
    Idle,
    Run,
    Stop = 10
}

class Rng72 {
    int state = 0;
    Rng72(int seed) { state = seed + 72; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt72 {
    int x = 0;
    int y = 0;
    Pt72(int px, int py) { x = px; y = py; }
    int manhattan(Pt72 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt72 other) { return x * other.x + y * other.y; }
}

class Worker72 {
    int total = 0;
    int count = 0;
    Mode72 mode = Mode72.Idle;
    string name = "worker72";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode72.Stop; } else { mode = Mode72.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner72 {
    int run() {
    auto rng = new Rng72(72);
    auto worker = new Worker72();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt72(1, 2);
    auto b = new Pt72(4, -6);
    int bias = 72;
    auto scale = n => n * bias;
    table t = { name = "m72", ["n"] = 72 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode73 {
    Idle,
    Run,
    Stop = 10
}

class Rng73 {
    int state = 0;
    Rng73(int seed) { state = seed + 73; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt73 {
    int x = 0;
    int y = 0;
    Pt73(int px, int py) { x = px; y = py; }
    int manhattan(Pt73 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt73 other) { return x * other.x + y * other.y; }
}

class Worker73 {
    int total = 0;
    int count = 0;
    Mode73 mode = Mode73.Idle;
    string name = "worker73";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode73.Stop; } else { mode = Mode73.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner73 {
    int run() {
    auto rng = new Rng73(73);
    auto worker = new Worker73();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt73(1, 2);
    auto b = new Pt73(4, -6);
    int bias = 73;
    auto scale = n => n * bias;
    table t = { name = "m73", ["n"] = 73 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode74 {
    Idle,
    Run,
    Stop = 10
}

class Rng74 {
    int state = 0;
    Rng74(int seed) { state = seed + 74; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt74 {
    int x = 0;
    int y = 0;
    Pt74(int px, int py) { x = px; y = py; }
    int manhattan(Pt74 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt74 other) { return x * other.x + y * other.y; }
}

class Worker74 {
    int total = 0;
    int count = 0;
    Mode74 mode = Mode74.Idle;
    string name = "worker74";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode74.Stop; } else { mode = Mode74.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner74 {
    int run() {
    auto rng = new Rng74(74);
    auto worker = new Worker74();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt74(1, 2);
    auto b = new Pt74(4, -6);
    int bias = 74;
    auto scale = n => n * bias;
    table t = { name = "m74", ["n"] = 74 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode75 {
    Idle,
    Run,
    Stop = 10
}

class Rng75 {
    int state = 0;
    Rng75(int seed) { state = seed + 75; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt75 {
    int x = 0;
    int y = 0;
    Pt75(int px, int py) { x = px; y = py; }
    int manhattan(Pt75 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt75 other) { return x * other.x + y * other.y; }
}

class Worker75 {
    int total = 0;
    int count = 0;
    Mode75 mode = Mode75.Idle;
    string name = "worker75";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode75.Stop; } else { mode = Mode75.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner75 {
    int run() {
    auto rng = new Rng75(75);
    auto worker = new Worker75();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt75(1, 2);
    auto b = new Pt75(4, -6);
    int bias = 75;
    auto scale = n => n * bias;
    table t = { name = "m75", ["n"] = 75 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode76 {
    Idle,
    Run,
    Stop = 10
}

class Rng76 {
    int state = 0;
    Rng76(int seed) { state = seed + 76; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt76 {
    int x = 0;
    int y = 0;
    Pt76(int px, int py) { x = px; y = py; }
    int manhattan(Pt76 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt76 other) { return x * other.x + y * other.y; }
}

class Worker76 {
    int total = 0;
    int count = 0;
    Mode76 mode = Mode76.Idle;
    string name = "worker76";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode76.Stop; } else { mode = Mode76.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner76 {
    int run() {
    auto rng = new Rng76(76);
    auto worker = new Worker76();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt76(1, 2);
    auto b = new Pt76(4, -6);
    int bias = 76;
    auto scale = n => n * bias;
    table t = { name = "m76", ["n"] = 76 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode77 {
    Idle,
    Run,
    Stop = 10
}

class Rng77 {
    int state = 0;
    Rng77(int seed) { state = seed + 77; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt77 {
    int x = 0;
    int y = 0;
    Pt77(int px, int py) { x = px; y = py; }
    int manhattan(Pt77 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt77 other) { return x * other.x + y * other.y; }
}

class Worker77 {
    int total = 0;
    int count = 0;
    Mode77 mode = Mode77.Idle;
    string name = "worker77";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode77.Stop; } else { mode = Mode77.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner77 {
    int run() {
    auto rng = new Rng77(77);
    auto worker = new Worker77();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt77(1, 2);
    auto b = new Pt77(4, -6);
    int bias = 77;
    auto scale = n => n * bias;
    table t = { name = "m77", ["n"] = 77 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode78 {
    Idle,
    Run,
    Stop = 10
}

class Rng78 {
    int state = 0;
    Rng78(int seed) { state = seed + 78; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt78 {
    int x = 0;
    int y = 0;
    Pt78(int px, int py) { x = px; y = py; }
    int manhattan(Pt78 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt78 other) { return x * other.x + y * other.y; }
}

class Worker78 {
    int total = 0;
    int count = 0;
    Mode78 mode = Mode78.Idle;
    string name = "worker78";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode78.Stop; } else { mode = Mode78.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner78 {
    int run() {
    auto rng = new Rng78(78);
    auto worker = new Worker78();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt78(1, 2);
    auto b = new Pt78(4, -6);
    int bias = 78;
    auto scale = n => n * bias;
    table t = { name = "m78", ["n"] = 78 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode79 {
    Idle,
    Run,
    Stop = 10
}

class Rng79 {
    int state = 0;
    Rng79(int seed) { state = seed + 79; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt79 {
    int x = 0;
    int y = 0;
    Pt79(int px, int py) { x = px; y = py; }
    int manhattan(Pt79 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt79 other) { return x * other.x + y * other.y; }
}

class Worker79 {
    int total = 0;
    int count = 0;
    Mode79 mode = Mode79.Idle;
    string name = "worker79";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode79.Stop; } else { mode = Mode79.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner79 {
    int run() {
    auto rng = new Rng79(79);
    auto worker = new Worker79();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt79(1, 2);
    auto b = new Pt79(4, -6);
    int bias = 79;
    auto scale = n => n * bias;
    table t = { name = "m79", ["n"] = 79 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode80 {
    Idle,
    Run,
    Stop = 10
}

class Rng80 {
    int state = 0;
    Rng80(int seed) { state = seed + 80; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt80 {
    int x = 0;
    int y = 0;
    Pt80(int px, int py) { x = px; y = py; }
    int manhattan(Pt80 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt80 other) { return x * other.x + y * other.y; }
}

class Worker80 {
    int total = 0;
    int count = 0;
    Mode80 mode = Mode80.Idle;
    string name = "worker80";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode80.Stop; } else { mode = Mode80.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner80 {
    int run() {
    auto rng = new Rng80(80);
    auto worker = new Worker80();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt80(1, 2);
    auto b = new Pt80(4, -6);
    int bias = 80;
    auto scale = n => n * bias;
    table t = { name = "m80", ["n"] = 80 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode81 {
    Idle,
    Run,
    Stop = 10
}

class Rng81 {
    int state = 0;
    Rng81(int seed) { state = seed + 81; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt81 {
    int x = 0;
    int y = 0;
    Pt81(int px, int py) { x = px; y = py; }
    int manhattan(Pt81 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt81 other) { return x * other.x + y * other.y; }
}

class Worker81 {
    int total = 0;
    int count = 0;
    Mode81 mode = Mode81.Idle;
    string name = "worker81";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode81.Stop; } else { mode = Mode81.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner81 {
    int run() {
    auto rng = new Rng81(81);
    auto worker = new Worker81();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt81(1, 2);
    auto b = new Pt81(4, -6);
    int bias = 81;
    auto scale = n => n * bias;
    table t = { name = "m81", ["n"] = 81 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode82 {
    Idle,
    Run,
    Stop = 10
}

class Rng82 {
    int state = 0;
    Rng82(int seed) { state = seed + 82; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt82 {
    int x = 0;
    int y = 0;
    Pt82(int px, int py) { x = px; y = py; }
    int manhattan(Pt82 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt82 other) { return x * other.x + y * other.y; }
}

class Worker82 {
    int total = 0;
    int count = 0;
    Mode82 mode = Mode82.Idle;
    string name = "worker82";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode82.Stop; } else { mode = Mode82.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner82 {
    int run() {
    auto rng = new Rng82(82);
    auto worker = new Worker82();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt82(1, 2);
    auto b = new Pt82(4, -6);
    int bias = 82;
    auto scale = n => n * bias;
    table t = { name = "m82", ["n"] = 82 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode83 {
    Idle,
    Run,
    Stop = 10
}

class Rng83 {
    int state = 0;
    Rng83(int seed) { state = seed + 83; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt83 {
    int x = 0;
    int y = 0;
    Pt83(int px, int py) { x = px; y = py; }
    int manhattan(Pt83 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt83 other) { return x * other.x + y * other.y; }
}

class Worker83 {
    int total = 0;
    int count = 0;
    Mode83 mode = Mode83.Idle;
    string name = "worker83";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode83.Stop; } else { mode = Mode83.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner83 {
    int run() {
    auto rng = new Rng83(83);
    auto worker = new Worker83();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt83(1, 2);
    auto b = new Pt83(4, -6);
    int bias = 83;
    auto scale = n => n * bias;
    table t = { name = "m83", ["n"] = 83 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode84 {
    Idle,
    Run,
    Stop = 10
}

class Rng84 {
    int state = 0;
    Rng84(int seed) { state = seed + 84; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt84 {
    int x = 0;
    int y = 0;
    Pt84(int px, int py) { x = px; y = py; }
    int manhattan(Pt84 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt84 other) { return x * other.x + y * other.y; }
}

class Worker84 {
    int total = 0;
    int count = 0;
    Mode84 mode = Mode84.Idle;
    string name = "worker84";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode84.Stop; } else { mode = Mode84.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner84 {
    int run() {
    auto rng = new Rng84(84);
    auto worker = new Worker84();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt84(1, 2);
    auto b = new Pt84(4, -6);
    int bias = 84;
    auto scale = n => n * bias;
    table t = { name = "m84", ["n"] = 84 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode85 {
    Idle,
    Run,
    Stop = 10
}

class Rng85 {
    int state = 0;
    Rng85(int seed) { state = seed + 85; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt85 {
    int x = 0;
    int y = 0;
    Pt85(int px, int py) { x = px; y = py; }
    int manhattan(Pt85 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt85 other) { return x * other.x + y * other.y; }
}

class Worker85 {
    int total = 0;
    int count = 0;
    Mode85 mode = Mode85.Idle;
    string name = "worker85";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode85.Stop; } else { mode = Mode85.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner85 {
    int run() {
    auto rng = new Rng85(85);
    auto worker = new Worker85();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt85(1, 2);
    auto b = new Pt85(4, -6);
    int bias = 85;
    auto scale = n => n * bias;
    table t = { name = "m85", ["n"] = 85 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode86 {
    Idle,
    Run,
    Stop = 10
}

class Rng86 {
    int state = 0;
    Rng86(int seed) { state = seed + 86; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt86 {
    int x = 0;
    int y = 0;
    Pt86(int px, int py) { x = px; y = py; }
    int manhattan(Pt86 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt86 other) { return x * other.x + y * other.y; }
}

class Worker86 {
    int total = 0;
    int count = 0;
    Mode86 mode = Mode86.Idle;
    string name = "worker86";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode86.Stop; } else { mode = Mode86.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner86 {
    int run() {
    auto rng = new Rng86(86);
    auto worker = new Worker86();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt86(1, 2);
    auto b = new Pt86(4, -6);
    int bias = 86;
    auto scale = n => n * bias;
    table t = { name = "m86", ["n"] = 86 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode87 {
    Idle,
    Run,
    Stop = 10
}

class Rng87 {
    int state = 0;
    Rng87(int seed) { state = seed + 87; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt87 {
    int x = 0;
    int y = 0;
    Pt87(int px, int py) { x = px; y = py; }
    int manhattan(Pt87 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt87 other) { return x * other.x + y * other.y; }
}

class Worker87 {
    int total = 0;
    int count = 0;
    Mode87 mode = Mode87.Idle;
    string name = "worker87";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode87.Stop; } else { mode = Mode87.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner87 {
    int run() {
    auto rng = new Rng87(87);
    auto worker = new Worker87();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt87(1, 2);
    auto b = new Pt87(4, -6);
    int bias = 87;
    auto scale = n => n * bias;
    table t = { name = "m87", ["n"] = 87 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode88 {
    Idle,
    Run,
    Stop = 10
}

class Rng88 {
    int state = 0;
    Rng88(int seed) { state = seed + 88; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt88 {
    int x = 0;
    int y = 0;
    Pt88(int px, int py) { x = px; y = py; }
    int manhattan(Pt88 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt88 other) { return x * other.x + y * other.y; }
}

class Worker88 {
    int total = 0;
    int count = 0;
    Mode88 mode = Mode88.Idle;
    string name = "worker88";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode88.Stop; } else { mode = Mode88.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner88 {
    int run() {
    auto rng = new Rng88(88);
    auto worker = new Worker88();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt88(1, 2);
    auto b = new Pt88(4, -6);
    int bias = 88;
    auto scale = n => n * bias;
    table t = { name = "m88", ["n"] = 88 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode89 {
    Idle,
    Run,
    Stop = 10
}

class Rng89 {
    int state = 0;
    Rng89(int seed) { state = seed + 89; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt89 {
    int x = 0;
    int y = 0;
    Pt89(int px, int py) { x = px; y = py; }
    int manhattan(Pt89 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt89 other) { return x * other.x + y * other.y; }
}

class Worker89 {
    int total = 0;
    int count = 0;
    Mode89 mode = Mode89.Idle;
    string name = "worker89";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode89.Stop; } else { mode = Mode89.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner89 {
    int run() {
    auto rng = new Rng89(89);
    auto worker = new Worker89();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt89(1, 2);
    auto b = new Pt89(4, -6);
    int bias = 89;
    auto scale = n => n * bias;
    table t = { name = "m89", ["n"] = 89 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode90 {
    Idle,
    Run,
    Stop = 10
}

class Rng90 {
    int state = 0;
    Rng90(int seed) { state = seed + 90; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt90 {
    int x = 0;
    int y = 0;
    Pt90(int px, int py) { x = px; y = py; }
    int manhattan(Pt90 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt90 other) { return x * other.x + y * other.y; }
}

class Worker90 {
    int total = 0;
    int count = 0;
    Mode90 mode = Mode90.Idle;
    string name = "worker90";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode90.Stop; } else { mode = Mode90.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner90 {
    int run() {
    auto rng = new Rng90(90);
    auto worker = new Worker90();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt90(1, 2);
    auto b = new Pt90(4, -6);
    int bias = 90;
    auto scale = n => n * bias;
    table t = { name = "m90", ["n"] = 90 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode91 {
    Idle,
    Run,
    Stop = 10
}

class Rng91 {
    int state = 0;
    Rng91(int seed) { state = seed + 91; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt91 {
    int x = 0;
    int y = 0;
    Pt91(int px, int py) { x = px; y = py; }
    int manhattan(Pt91 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt91 other) { return x * other.x + y * other.y; }
}

class Worker91 {
    int total = 0;
    int count = 0;
    Mode91 mode = Mode91.Idle;
    string name = "worker91";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode91.Stop; } else { mode = Mode91.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner91 {
    int run() {
    auto rng = new Rng91(91);
    auto worker = new Worker91();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt91(1, 2);
    auto b = new Pt91(4, -6);
    int bias = 91;
    auto scale = n => n * bias;
    table t = { name = "m91", ["n"] = 91 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode92 {
    Idle,
    Run,
    Stop = 10
}

class Rng92 {
    int state = 0;
    Rng92(int seed) { state = seed + 92; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt92 {
    int x = 0;
    int y = 0;
    Pt92(int px, int py) { x = px; y = py; }
    int manhattan(Pt92 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt92 other) { return x * other.x + y * other.y; }
}

class Worker92 {
    int total = 0;
    int count = 0;
    Mode92 mode = Mode92.Idle;
    string name = "worker92";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode92.Stop; } else { mode = Mode92.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner92 {
    int run() {
    auto rng = new Rng92(92);
    auto worker = new Worker92();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt92(1, 2);
    auto b = new Pt92(4, -6);
    int bias = 92;
    auto scale = n => n * bias;
    table t = { name = "m92", ["n"] = 92 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode93 {
    Idle,
    Run,
    Stop = 10
}

class Rng93 {
    int state = 0;
    Rng93(int seed) { state = seed + 93; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt93 {
    int x = 0;
    int y = 0;
    Pt93(int px, int py) { x = px; y = py; }
    int manhattan(Pt93 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt93 other) { return x * other.x + y * other.y; }
}

class Worker93 {
    int total = 0;
    int count = 0;
    Mode93 mode = Mode93.Idle;
    string name = "worker93";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode93.Stop; } else { mode = Mode93.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner93 {
    int run() {
    auto rng = new Rng93(93);
    auto worker = new Worker93();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt93(1, 2);
    auto b = new Pt93(4, -6);
    int bias = 93;
    auto scale = n => n * bias;
    table t = { name = "m93", ["n"] = 93 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode94 {
    Idle,
    Run,
    Stop = 10
}

class Rng94 {
    int state = 0;
    Rng94(int seed) { state = seed + 94; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt94 {
    int x = 0;
    int y = 0;
    Pt94(int px, int py) { x = px; y = py; }
    int manhattan(Pt94 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt94 other) { return x * other.x + y * other.y; }
}

class Worker94 {
    int total = 0;
    int count = 0;
    Mode94 mode = Mode94.Idle;
    string name = "worker94";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode94.Stop; } else { mode = Mode94.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner94 {
    int run() {
    auto rng = new Rng94(94);
    auto worker = new Worker94();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt94(1, 2);
    auto b = new Pt94(4, -6);
    int bias = 94;
    auto scale = n => n * bias;
    table t = { name = "m94", ["n"] = 94 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode95 {
    Idle,
    Run,
    Stop = 10
}

class Rng95 {
    int state = 0;
    Rng95(int seed) { state = seed + 95; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt95 {
    int x = 0;
    int y = 0;
    Pt95(int px, int py) { x = px; y = py; }
    int manhattan(Pt95 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt95 other) { return x * other.x + y * other.y; }
}

class Worker95 {
    int total = 0;
    int count = 0;
    Mode95 mode = Mode95.Idle;
    string name = "worker95";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode95.Stop; } else { mode = Mode95.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner95 {
    int run() {
    auto rng = new Rng95(95);
    auto worker = new Worker95();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt95(1, 2);
    auto b = new Pt95(4, -6);
    int bias = 95;
    auto scale = n => n * bias;
    table t = { name = "m95", ["n"] = 95 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode96 {
    Idle,
    Run,
    Stop = 10
}

class Rng96 {
    int state = 0;
    Rng96(int seed) { state = seed + 96; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt96 {
    int x = 0;
    int y = 0;
    Pt96(int px, int py) { x = px; y = py; }
    int manhattan(Pt96 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt96 other) { return x * other.x + y * other.y; }
}

class Worker96 {
    int total = 0;
    int count = 0;
    Mode96 mode = Mode96.Idle;
    string name = "worker96";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode96.Stop; } else { mode = Mode96.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner96 {
    int run() {
    auto rng = new Rng96(96);
    auto worker = new Worker96();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt96(1, 2);
    auto b = new Pt96(4, -6);
    int bias = 96;
    auto scale = n => n * bias;
    table t = { name = "m96", ["n"] = 96 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode97 {
    Idle,
    Run,
    Stop = 10
}

class Rng97 {
    int state = 0;
    Rng97(int seed) { state = seed + 97; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt97 {
    int x = 0;
    int y = 0;
    Pt97(int px, int py) { x = px; y = py; }
    int manhattan(Pt97 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt97 other) { return x * other.x + y * other.y; }
}

class Worker97 {
    int total = 0;
    int count = 0;
    Mode97 mode = Mode97.Idle;
    string name = "worker97";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode97.Stop; } else { mode = Mode97.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner97 {
    int run() {
    auto rng = new Rng97(97);
    auto worker = new Worker97();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt97(1, 2);
    auto b = new Pt97(4, -6);
    int bias = 97;
    auto scale = n => n * bias;
    table t = { name = "m97", ["n"] = 97 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode98 {
    Idle,
    Run,
    Stop = 10
}

class Rng98 {
    int state = 0;
    Rng98(int seed) { state = seed + 98; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt98 {
    int x = 0;
    int y = 0;
    Pt98(int px, int py) { x = px; y = py; }
    int manhattan(Pt98 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt98 other) { return x * other.x + y * other.y; }
}

class Worker98 {
    int total = 0;
    int count = 0;
    Mode98 mode = Mode98.Idle;
    string name = "worker98";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode98.Stop; } else { mode = Mode98.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner98 {
    int run() {
    auto rng = new Rng98(98);
    auto worker = new Worker98();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt98(1, 2);
    auto b = new Pt98(4, -6);
    int bias = 98;
    auto scale = n => n * bias;
    table t = { name = "m98", ["n"] = 98 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode99 {
    Idle,
    Run,
    Stop = 10
}

class Rng99 {
    int state = 0;
    Rng99(int seed) { state = seed + 99; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt99 {
    int x = 0;
    int y = 0;
    Pt99(int px, int py) { x = px; y = py; }
    int manhattan(Pt99 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt99 other) { return x * other.x + y * other.y; }
}

class Worker99 {
    int total = 0;
    int count = 0;
    Mode99 mode = Mode99.Idle;
    string name = "worker99";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode99.Stop; } else { mode = Mode99.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner99 {
    int run() {
    auto rng = new Rng99(99);
    auto worker = new Worker99();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt99(1, 2);
    auto b = new Pt99(4, -6);
    int bias = 99;
    auto scale = n => n * bias;
    table t = { name = "m99", ["n"] = 99 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode100 {
    Idle,
    Run,
    Stop = 10
}

class Rng100 {
    int state = 0;
    Rng100(int seed) { state = seed + 100; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt100 {
    int x = 0;
    int y = 0;
    Pt100(int px, int py) { x = px; y = py; }
    int manhattan(Pt100 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt100 other) { return x * other.x + y * other.y; }
}

class Worker100 {
    int total = 0;
    int count = 0;
    Mode100 mode = Mode100.Idle;
    string name = "worker100";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode100.Stop; } else { mode = Mode100.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner100 {
    int run() {
    auto rng = new Rng100(100);
    auto worker = new Worker100();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt100(1, 2);
    auto b = new Pt100(4, -6);
    int bias = 100;
    auto scale = n => n * bias;
    table t = { name = "m100", ["n"] = 100 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode101 {
    Idle,
    Run,
    Stop = 10
}

class Rng101 {
    int state = 0;
    Rng101(int seed) { state = seed + 101; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt101 {
    int x = 0;
    int y = 0;
    Pt101(int px, int py) { x = px; y = py; }
    int manhattan(Pt101 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt101 other) { return x * other.x + y * other.y; }
}

class Worker101 {
    int total = 0;
    int count = 0;
    Mode101 mode = Mode101.Idle;
    string name = "worker101";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode101.Stop; } else { mode = Mode101.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner101 {
    int run() {
    auto rng = new Rng101(101);
    auto worker = new Worker101();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt101(1, 2);
    auto b = new Pt101(4, -6);
    int bias = 101;
    auto scale = n => n * bias;
    table t = { name = "m101", ["n"] = 101 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode102 {
    Idle,
    Run,
    Stop = 10
}

class Rng102 {
    int state = 0;
    Rng102(int seed) { state = seed + 102; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt102 {
    int x = 0;
    int y = 0;
    Pt102(int px, int py) { x = px; y = py; }
    int manhattan(Pt102 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt102 other) { return x * other.x + y * other.y; }
}

class Worker102 {
    int total = 0;
    int count = 0;
    Mode102 mode = Mode102.Idle;
    string name = "worker102";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode102.Stop; } else { mode = Mode102.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner102 {
    int run() {
    auto rng = new Rng102(102);
    auto worker = new Worker102();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt102(1, 2);
    auto b = new Pt102(4, -6);
    int bias = 102;
    auto scale = n => n * bias;
    table t = { name = "m102", ["n"] = 102 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode103 {
    Idle,
    Run,
    Stop = 10
}

class Rng103 {
    int state = 0;
    Rng103(int seed) { state = seed + 103; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt103 {
    int x = 0;
    int y = 0;
    Pt103(int px, int py) { x = px; y = py; }
    int manhattan(Pt103 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt103 other) { return x * other.x + y * other.y; }
}

class Worker103 {
    int total = 0;
    int count = 0;
    Mode103 mode = Mode103.Idle;
    string name = "worker103";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode103.Stop; } else { mode = Mode103.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner103 {
    int run() {
    auto rng = new Rng103(103);
    auto worker = new Worker103();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt103(1, 2);
    auto b = new Pt103(4, -6);
    int bias = 103;
    auto scale = n => n * bias;
    table t = { name = "m103", ["n"] = 103 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode104 {
    Idle,
    Run,
    Stop = 10
}

class Rng104 {
    int state = 0;
    Rng104(int seed) { state = seed + 104; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt104 {
    int x = 0;
    int y = 0;
    Pt104(int px, int py) { x = px; y = py; }
    int manhattan(Pt104 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt104 other) { return x * other.x + y * other.y; }
}

class Worker104 {
    int total = 0;
    int count = 0;
    Mode104 mode = Mode104.Idle;
    string name = "worker104";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode104.Stop; } else { mode = Mode104.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner104 {
    int run() {
    auto rng = new Rng104(104);
    auto worker = new Worker104();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt104(1, 2);
    auto b = new Pt104(4, -6);
    int bias = 104;
    auto scale = n => n * bias;
    table t = { name = "m104", ["n"] = 104 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode105 {
    Idle,
    Run,
    Stop = 10
}

class Rng105 {
    int state = 0;
    Rng105(int seed) { state = seed + 105; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt105 {
    int x = 0;
    int y = 0;
    Pt105(int px, int py) { x = px; y = py; }
    int manhattan(Pt105 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt105 other) { return x * other.x + y * other.y; }
}

class Worker105 {
    int total = 0;
    int count = 0;
    Mode105 mode = Mode105.Idle;
    string name = "worker105";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode105.Stop; } else { mode = Mode105.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner105 {
    int run() {
    auto rng = new Rng105(105);
    auto worker = new Worker105();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt105(1, 2);
    auto b = new Pt105(4, -6);
    int bias = 105;
    auto scale = n => n * bias;
    table t = { name = "m105", ["n"] = 105 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode106 {
    Idle,
    Run,
    Stop = 10
}

class Rng106 {
    int state = 0;
    Rng106(int seed) { state = seed + 106; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt106 {
    int x = 0;
    int y = 0;
    Pt106(int px, int py) { x = px; y = py; }
    int manhattan(Pt106 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt106 other) { return x * other.x + y * other.y; }
}

class Worker106 {
    int total = 0;
    int count = 0;
    Mode106 mode = Mode106.Idle;
    string name = "worker106";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode106.Stop; } else { mode = Mode106.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner106 {
    int run() {
    auto rng = new Rng106(106);
    auto worker = new Worker106();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt106(1, 2);
    auto b = new Pt106(4, -6);
    int bias = 106;
    auto scale = n => n * bias;
    table t = { name = "m106", ["n"] = 106 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode107 {
    Idle,
    Run,
    Stop = 10
}

class Rng107 {
    int state = 0;
    Rng107(int seed) { state = seed + 107; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt107 {
    int x = 0;
    int y = 0;
    Pt107(int px, int py) { x = px; y = py; }
    int manhattan(Pt107 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt107 other) { return x * other.x + y * other.y; }
}

class Worker107 {
    int total = 0;
    int count = 0;
    Mode107 mode = Mode107.Idle;
    string name = "worker107";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode107.Stop; } else { mode = Mode107.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner107 {
    int run() {
    auto rng = new Rng107(107);
    auto worker = new Worker107();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt107(1, 2);
    auto b = new Pt107(4, -6);
    int bias = 107;
    auto scale = n => n * bias;
    table t = { name = "m107", ["n"] = 107 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode108 {
    Idle,
    Run,
    Stop = 10
}

class Rng108 {
    int state = 0;
    Rng108(int seed) { state = seed + 108; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt108 {
    int x = 0;
    int y = 0;
    Pt108(int px, int py) { x = px; y = py; }
    int manhattan(Pt108 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt108 other) { return x * other.x + y * other.y; }
}

class Worker108 {
    int total = 0;
    int count = 0;
    Mode108 mode = Mode108.Idle;
    string name = "worker108";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode108.Stop; } else { mode = Mode108.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner108 {
    int run() {
    auto rng = new Rng108(108);
    auto worker = new Worker108();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt108(1, 2);
    auto b = new Pt108(4, -6);
    int bias = 108;
    auto scale = n => n * bias;
    table t = { name = "m108", ["n"] = 108 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode109 {
    Idle,
    Run,
    Stop = 10
}

class Rng109 {
    int state = 0;
    Rng109(int seed) { state = seed + 109; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt109 {
    int x = 0;
    int y = 0;
    Pt109(int px, int py) { x = px; y = py; }
    int manhattan(Pt109 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt109 other) { return x * other.x + y * other.y; }
}

class Worker109 {
    int total = 0;
    int count = 0;
    Mode109 mode = Mode109.Idle;
    string name = "worker109";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode109.Stop; } else { mode = Mode109.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner109 {
    int run() {
    auto rng = new Rng109(109);
    auto worker = new Worker109();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt109(1, 2);
    auto b = new Pt109(4, -6);
    int bias = 109;
    auto scale = n => n * bias;
    table t = { name = "m109", ["n"] = 109 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode110 {
    Idle,
    Run,
    Stop = 10
}

class Rng110 {
    int state = 0;
    Rng110(int seed) { state = seed + 110; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt110 {
    int x = 0;
    int y = 0;
    Pt110(int px, int py) { x = px; y = py; }
    int manhattan(Pt110 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt110 other) { return x * other.x + y * other.y; }
}

class Worker110 {
    int total = 0;
    int count = 0;
    Mode110 mode = Mode110.Idle;
    string name = "worker110";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode110.Stop; } else { mode = Mode110.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner110 {
    int run() {
    auto rng = new Rng110(110);
    auto worker = new Worker110();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt110(1, 2);
    auto b = new Pt110(4, -6);
    int bias = 110;
    auto scale = n => n * bias;
    table t = { name = "m110", ["n"] = 110 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode111 {
    Idle,
    Run,
    Stop = 10
}

class Rng111 {
    int state = 0;
    Rng111(int seed) { state = seed + 111; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt111 {
    int x = 0;
    int y = 0;
    Pt111(int px, int py) { x = px; y = py; }
    int manhattan(Pt111 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt111 other) { return x * other.x + y * other.y; }
}

class Worker111 {
    int total = 0;
    int count = 0;
    Mode111 mode = Mode111.Idle;
    string name = "worker111";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode111.Stop; } else { mode = Mode111.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner111 {
    int run() {
    auto rng = new Rng111(111);
    auto worker = new Worker111();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt111(1, 2);
    auto b = new Pt111(4, -6);
    int bias = 111;
    auto scale = n => n * bias;
    table t = { name = "m111", ["n"] = 111 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode112 {
    Idle,
    Run,
    Stop = 10
}

class Rng112 {
    int state = 0;
    Rng112(int seed) { state = seed + 112; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt112 {
    int x = 0;
    int y = 0;
    Pt112(int px, int py) { x = px; y = py; }
    int manhattan(Pt112 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt112 other) { return x * other.x + y * other.y; }
}

class Worker112 {
    int total = 0;
    int count = 0;
    Mode112 mode = Mode112.Idle;
    string name = "worker112";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode112.Stop; } else { mode = Mode112.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner112 {
    int run() {
    auto rng = new Rng112(112);
    auto worker = new Worker112();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt112(1, 2);
    auto b = new Pt112(4, -6);
    int bias = 112;
    auto scale = n => n * bias;
    table t = { name = "m112", ["n"] = 112 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode113 {
    Idle,
    Run,
    Stop = 10
}

class Rng113 {
    int state = 0;
    Rng113(int seed) { state = seed + 113; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt113 {
    int x = 0;
    int y = 0;
    Pt113(int px, int py) { x = px; y = py; }
    int manhattan(Pt113 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt113 other) { return x * other.x + y * other.y; }
}

class Worker113 {
    int total = 0;
    int count = 0;
    Mode113 mode = Mode113.Idle;
    string name = "worker113";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode113.Stop; } else { mode = Mode113.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner113 {
    int run() {
    auto rng = new Rng113(113);
    auto worker = new Worker113();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt113(1, 2);
    auto b = new Pt113(4, -6);
    int bias = 113;
    auto scale = n => n * bias;
    table t = { name = "m113", ["n"] = 113 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode114 {
    Idle,
    Run,
    Stop = 10
}

class Rng114 {
    int state = 0;
    Rng114(int seed) { state = seed + 114; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt114 {
    int x = 0;
    int y = 0;
    Pt114(int px, int py) { x = px; y = py; }
    int manhattan(Pt114 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt114 other) { return x * other.x + y * other.y; }
}

class Worker114 {
    int total = 0;
    int count = 0;
    Mode114 mode = Mode114.Idle;
    string name = "worker114";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode114.Stop; } else { mode = Mode114.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner114 {
    int run() {
    auto rng = new Rng114(114);
    auto worker = new Worker114();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt114(1, 2);
    auto b = new Pt114(4, -6);
    int bias = 114;
    auto scale = n => n * bias;
    table t = { name = "m114", ["n"] = 114 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode115 {
    Idle,
    Run,
    Stop = 10
}

class Rng115 {
    int state = 0;
    Rng115(int seed) { state = seed + 115; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt115 {
    int x = 0;
    int y = 0;
    Pt115(int px, int py) { x = px; y = py; }
    int manhattan(Pt115 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt115 other) { return x * other.x + y * other.y; }
}

class Worker115 {
    int total = 0;
    int count = 0;
    Mode115 mode = Mode115.Idle;
    string name = "worker115";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode115.Stop; } else { mode = Mode115.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner115 {
    int run() {
    auto rng = new Rng115(115);
    auto worker = new Worker115();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt115(1, 2);
    auto b = new Pt115(4, -6);
    int bias = 115;
    auto scale = n => n * bias;
    table t = { name = "m115", ["n"] = 115 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode116 {
    Idle,
    Run,
    Stop = 10
}

class Rng116 {
    int state = 0;
    Rng116(int seed) { state = seed + 116; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt116 {
    int x = 0;
    int y = 0;
    Pt116(int px, int py) { x = px; y = py; }
    int manhattan(Pt116 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt116 other) { return x * other.x + y * other.y; }
}

class Worker116 {
    int total = 0;
    int count = 0;
    Mode116 mode = Mode116.Idle;
    string name = "worker116";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode116.Stop; } else { mode = Mode116.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner116 {
    int run() {
    auto rng = new Rng116(116);
    auto worker = new Worker116();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt116(1, 2);
    auto b = new Pt116(4, -6);
    int bias = 116;
    auto scale = n => n * bias;
    table t = { name = "m116", ["n"] = 116 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode117 {
    Idle,
    Run,
    Stop = 10
}

class Rng117 {
    int state = 0;
    Rng117(int seed) { state = seed + 117; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt117 {
    int x = 0;
    int y = 0;
    Pt117(int px, int py) { x = px; y = py; }
    int manhattan(Pt117 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt117 other) { return x * other.x + y * other.y; }
}

class Worker117 {
    int total = 0;
    int count = 0;
    Mode117 mode = Mode117.Idle;
    string name = "worker117";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode117.Stop; } else { mode = Mode117.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner117 {
    int run() {
    auto rng = new Rng117(117);
    auto worker = new Worker117();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt117(1, 2);
    auto b = new Pt117(4, -6);
    int bias = 117;
    auto scale = n => n * bias;
    table t = { name = "m117", ["n"] = 117 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode118 {
    Idle,
    Run,
    Stop = 10
}

class Rng118 {
    int state = 0;
    Rng118(int seed) { state = seed + 118; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt118 {
    int x = 0;
    int y = 0;
    Pt118(int px, int py) { x = px; y = py; }
    int manhattan(Pt118 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt118 other) { return x * other.x + y * other.y; }
}

class Worker118 {
    int total = 0;
    int count = 0;
    Mode118 mode = Mode118.Idle;
    string name = "worker118";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode118.Stop; } else { mode = Mode118.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner118 {
    int run() {
    auto rng = new Rng118(118);
    auto worker = new Worker118();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt118(1, 2);
    auto b = new Pt118(4, -6);
    int bias = 118;
    auto scale = n => n * bias;
    table t = { name = "m118", ["n"] = 118 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode119 {
    Idle,
    Run,
    Stop = 10
}

class Rng119 {
    int state = 0;
    Rng119(int seed) { state = seed + 119; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt119 {
    int x = 0;
    int y = 0;
    Pt119(int px, int py) { x = px; y = py; }
    int manhattan(Pt119 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt119 other) { return x * other.x + y * other.y; }
}

class Worker119 {
    int total = 0;
    int count = 0;
    Mode119 mode = Mode119.Idle;
    string name = "worker119";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode119.Stop; } else { mode = Mode119.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner119 {
    int run() {
    auto rng = new Rng119(119);
    auto worker = new Worker119();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt119(1, 2);
    auto b = new Pt119(4, -6);
    int bias = 119;
    auto scale = n => n * bias;
    table t = { name = "m119", ["n"] = 119 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode120 {
    Idle,
    Run,
    Stop = 10
}

class Rng120 {
    int state = 0;
    Rng120(int seed) { state = seed + 120; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt120 {
    int x = 0;
    int y = 0;
    Pt120(int px, int py) { x = px; y = py; }
    int manhattan(Pt120 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt120 other) { return x * other.x + y * other.y; }
}

class Worker120 {
    int total = 0;
    int count = 0;
    Mode120 mode = Mode120.Idle;
    string name = "worker120";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode120.Stop; } else { mode = Mode120.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner120 {
    int run() {
    auto rng = new Rng120(120);
    auto worker = new Worker120();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt120(1, 2);
    auto b = new Pt120(4, -6);
    int bias = 120;
    auto scale = n => n * bias;
    table t = { name = "m120", ["n"] = 120 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode121 {
    Idle,
    Run,
    Stop = 10
}

class Rng121 {
    int state = 0;
    Rng121(int seed) { state = seed + 121; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt121 {
    int x = 0;
    int y = 0;
    Pt121(int px, int py) { x = px; y = py; }
    int manhattan(Pt121 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt121 other) { return x * other.x + y * other.y; }
}

class Worker121 {
    int total = 0;
    int count = 0;
    Mode121 mode = Mode121.Idle;
    string name = "worker121";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode121.Stop; } else { mode = Mode121.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner121 {
    int run() {
    auto rng = new Rng121(121);
    auto worker = new Worker121();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt121(1, 2);
    auto b = new Pt121(4, -6);
    int bias = 121;
    auto scale = n => n * bias;
    table t = { name = "m121", ["n"] = 121 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode122 {
    Idle,
    Run,
    Stop = 10
}

class Rng122 {
    int state = 0;
    Rng122(int seed) { state = seed + 122; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt122 {
    int x = 0;
    int y = 0;
    Pt122(int px, int py) { x = px; y = py; }
    int manhattan(Pt122 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt122 other) { return x * other.x + y * other.y; }
}

class Worker122 {
    int total = 0;
    int count = 0;
    Mode122 mode = Mode122.Idle;
    string name = "worker122";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode122.Stop; } else { mode = Mode122.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner122 {
    int run() {
    auto rng = new Rng122(122);
    auto worker = new Worker122();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt122(1, 2);
    auto b = new Pt122(4, -6);
    int bias = 122;
    auto scale = n => n * bias;
    table t = { name = "m122", ["n"] = 122 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode123 {
    Idle,
    Run,
    Stop = 10
}

class Rng123 {
    int state = 0;
    Rng123(int seed) { state = seed + 123; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt123 {
    int x = 0;
    int y = 0;
    Pt123(int px, int py) { x = px; y = py; }
    int manhattan(Pt123 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt123 other) { return x * other.x + y * other.y; }
}

class Worker123 {
    int total = 0;
    int count = 0;
    Mode123 mode = Mode123.Idle;
    string name = "worker123";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode123.Stop; } else { mode = Mode123.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner123 {
    int run() {
    auto rng = new Rng123(123);
    auto worker = new Worker123();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt123(1, 2);
    auto b = new Pt123(4, -6);
    int bias = 123;
    auto scale = n => n * bias;
    table t = { name = "m123", ["n"] = 123 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode124 {
    Idle,
    Run,
    Stop = 10
}

class Rng124 {
    int state = 0;
    Rng124(int seed) { state = seed + 124; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt124 {
    int x = 0;
    int y = 0;
    Pt124(int px, int py) { x = px; y = py; }
    int manhattan(Pt124 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt124 other) { return x * other.x + y * other.y; }
}

class Worker124 {
    int total = 0;
    int count = 0;
    Mode124 mode = Mode124.Idle;
    string name = "worker124";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode124.Stop; } else { mode = Mode124.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner124 {
    int run() {
    auto rng = new Rng124(124);
    auto worker = new Worker124();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt124(1, 2);
    auto b = new Pt124(4, -6);
    int bias = 124;
    auto scale = n => n * bias;
    table t = { name = "m124", ["n"] = 124 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode125 {
    Idle,
    Run,
    Stop = 10
}

class Rng125 {
    int state = 0;
    Rng125(int seed) { state = seed + 125; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt125 {
    int x = 0;
    int y = 0;
    Pt125(int px, int py) { x = px; y = py; }
    int manhattan(Pt125 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt125 other) { return x * other.x + y * other.y; }
}

class Worker125 {
    int total = 0;
    int count = 0;
    Mode125 mode = Mode125.Idle;
    string name = "worker125";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode125.Stop; } else { mode = Mode125.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner125 {
    int run() {
    auto rng = new Rng125(125);
    auto worker = new Worker125();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt125(1, 2);
    auto b = new Pt125(4, -6);
    int bias = 125;
    auto scale = n => n * bias;
    table t = { name = "m125", ["n"] = 125 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode126 {
    Idle,
    Run,
    Stop = 10
}

class Rng126 {
    int state = 0;
    Rng126(int seed) { state = seed + 126; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt126 {
    int x = 0;
    int y = 0;
    Pt126(int px, int py) { x = px; y = py; }
    int manhattan(Pt126 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt126 other) { return x * other.x + y * other.y; }
}

class Worker126 {
    int total = 0;
    int count = 0;
    Mode126 mode = Mode126.Idle;
    string name = "worker126";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode126.Stop; } else { mode = Mode126.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner126 {
    int run() {
    auto rng = new Rng126(126);
    auto worker = new Worker126();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt126(1, 2);
    auto b = new Pt126(4, -6);
    int bias = 126;
    auto scale = n => n * bias;
    table t = { name = "m126", ["n"] = 126 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode127 {
    Idle,
    Run,
    Stop = 10
}

class Rng127 {
    int state = 0;
    Rng127(int seed) { state = seed + 127; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt127 {
    int x = 0;
    int y = 0;
    Pt127(int px, int py) { x = px; y = py; }
    int manhattan(Pt127 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt127 other) { return x * other.x + y * other.y; }
}

class Worker127 {
    int total = 0;
    int count = 0;
    Mode127 mode = Mode127.Idle;
    string name = "worker127";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode127.Stop; } else { mode = Mode127.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner127 {
    int run() {
    auto rng = new Rng127(127);
    auto worker = new Worker127();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt127(1, 2);
    auto b = new Pt127(4, -6);
    int bias = 127;
    auto scale = n => n * bias;
    table t = { name = "m127", ["n"] = 127 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode128 {
    Idle,
    Run,
    Stop = 10
}

class Rng128 {
    int state = 0;
    Rng128(int seed) { state = seed + 128; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt128 {
    int x = 0;
    int y = 0;
    Pt128(int px, int py) { x = px; y = py; }
    int manhattan(Pt128 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt128 other) { return x * other.x + y * other.y; }
}

class Worker128 {
    int total = 0;
    int count = 0;
    Mode128 mode = Mode128.Idle;
    string name = "worker128";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode128.Stop; } else { mode = Mode128.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner128 {
    int run() {
    auto rng = new Rng128(128);
    auto worker = new Worker128();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt128(1, 2);
    auto b = new Pt128(4, -6);
    int bias = 128;
    auto scale = n => n * bias;
    table t = { name = "m128", ["n"] = 128 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode129 {
    Idle,
    Run,
    Stop = 10
}

class Rng129 {
    int state = 0;
    Rng129(int seed) { state = seed + 129; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt129 {
    int x = 0;
    int y = 0;
    Pt129(int px, int py) { x = px; y = py; }
    int manhattan(Pt129 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt129 other) { return x * other.x + y * other.y; }
}

class Worker129 {
    int total = 0;
    int count = 0;
    Mode129 mode = Mode129.Idle;
    string name = "worker129";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode129.Stop; } else { mode = Mode129.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner129 {
    int run() {
    auto rng = new Rng129(129);
    auto worker = new Worker129();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt129(1, 2);
    auto b = new Pt129(4, -6);
    int bias = 129;
    auto scale = n => n * bias;
    table t = { name = "m129", ["n"] = 129 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode130 {
    Idle,
    Run,
    Stop = 10
}

class Rng130 {
    int state = 0;
    Rng130(int seed) { state = seed + 130; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt130 {
    int x = 0;
    int y = 0;
    Pt130(int px, int py) { x = px; y = py; }
    int manhattan(Pt130 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt130 other) { return x * other.x + y * other.y; }
}

class Worker130 {
    int total = 0;
    int count = 0;
    Mode130 mode = Mode130.Idle;
    string name = "worker130";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode130.Stop; } else { mode = Mode130.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner130 {
    int run() {
    auto rng = new Rng130(130);
    auto worker = new Worker130();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt130(1, 2);
    auto b = new Pt130(4, -6);
    int bias = 130;
    auto scale = n => n * bias;
    table t = { name = "m130", ["n"] = 130 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode131 {
    Idle,
    Run,
    Stop = 10
}

class Rng131 {
    int state = 0;
    Rng131(int seed) { state = seed + 131; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt131 {
    int x = 0;
    int y = 0;
    Pt131(int px, int py) { x = px; y = py; }
    int manhattan(Pt131 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt131 other) { return x * other.x + y * other.y; }
}

class Worker131 {
    int total = 0;
    int count = 0;
    Mode131 mode = Mode131.Idle;
    string name = "worker131";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode131.Stop; } else { mode = Mode131.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner131 {
    int run() {
    auto rng = new Rng131(131);
    auto worker = new Worker131();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt131(1, 2);
    auto b = new Pt131(4, -6);
    int bias = 131;
    auto scale = n => n * bias;
    table t = { name = "m131", ["n"] = 131 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode132 {
    Idle,
    Run,
    Stop = 10
}

class Rng132 {
    int state = 0;
    Rng132(int seed) { state = seed + 132; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt132 {
    int x = 0;
    int y = 0;
    Pt132(int px, int py) { x = px; y = py; }
    int manhattan(Pt132 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt132 other) { return x * other.x + y * other.y; }
}

class Worker132 {
    int total = 0;
    int count = 0;
    Mode132 mode = Mode132.Idle;
    string name = "worker132";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode132.Stop; } else { mode = Mode132.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner132 {
    int run() {
    auto rng = new Rng132(132);
    auto worker = new Worker132();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt132(1, 2);
    auto b = new Pt132(4, -6);
    int bias = 132;
    auto scale = n => n * bias;
    table t = { name = "m132", ["n"] = 132 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode133 {
    Idle,
    Run,
    Stop = 10
}

class Rng133 {
    int state = 0;
    Rng133(int seed) { state = seed + 133; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt133 {
    int x = 0;
    int y = 0;
    Pt133(int px, int py) { x = px; y = py; }
    int manhattan(Pt133 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt133 other) { return x * other.x + y * other.y; }
}

class Worker133 {
    int total = 0;
    int count = 0;
    Mode133 mode = Mode133.Idle;
    string name = "worker133";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode133.Stop; } else { mode = Mode133.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner133 {
    int run() {
    auto rng = new Rng133(133);
    auto worker = new Worker133();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt133(1, 2);
    auto b = new Pt133(4, -6);
    int bias = 133;
    auto scale = n => n * bias;
    table t = { name = "m133", ["n"] = 133 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode134 {
    Idle,
    Run,
    Stop = 10
}

class Rng134 {
    int state = 0;
    Rng134(int seed) { state = seed + 134; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt134 {
    int x = 0;
    int y = 0;
    Pt134(int px, int py) { x = px; y = py; }
    int manhattan(Pt134 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt134 other) { return x * other.x + y * other.y; }
}

class Worker134 {
    int total = 0;
    int count = 0;
    Mode134 mode = Mode134.Idle;
    string name = "worker134";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode134.Stop; } else { mode = Mode134.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner134 {
    int run() {
    auto rng = new Rng134(134);
    auto worker = new Worker134();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt134(1, 2);
    auto b = new Pt134(4, -6);
    int bias = 134;
    auto scale = n => n * bias;
    table t = { name = "m134", ["n"] = 134 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode135 {
    Idle,
    Run,
    Stop = 10
}

class Rng135 {
    int state = 0;
    Rng135(int seed) { state = seed + 135; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt135 {
    int x = 0;
    int y = 0;
    Pt135(int px, int py) { x = px; y = py; }
    int manhattan(Pt135 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt135 other) { return x * other.x + y * other.y; }
}

class Worker135 {
    int total = 0;
    int count = 0;
    Mode135 mode = Mode135.Idle;
    string name = "worker135";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode135.Stop; } else { mode = Mode135.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner135 {
    int run() {
    auto rng = new Rng135(135);
    auto worker = new Worker135();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt135(1, 2);
    auto b = new Pt135(4, -6);
    int bias = 135;
    auto scale = n => n * bias;
    table t = { name = "m135", ["n"] = 135 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode136 {
    Idle,
    Run,
    Stop = 10
}

class Rng136 {
    int state = 0;
    Rng136(int seed) { state = seed + 136; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt136 {
    int x = 0;
    int y = 0;
    Pt136(int px, int py) { x = px; y = py; }
    int manhattan(Pt136 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt136 other) { return x * other.x + y * other.y; }
}

class Worker136 {
    int total = 0;
    int count = 0;
    Mode136 mode = Mode136.Idle;
    string name = "worker136";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode136.Stop; } else { mode = Mode136.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner136 {
    int run() {
    auto rng = new Rng136(136);
    auto worker = new Worker136();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt136(1, 2);
    auto b = new Pt136(4, -6);
    int bias = 136;
    auto scale = n => n * bias;
    table t = { name = "m136", ["n"] = 136 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode137 {
    Idle,
    Run,
    Stop = 10
}

class Rng137 {
    int state = 0;
    Rng137(int seed) { state = seed + 137; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt137 {
    int x = 0;
    int y = 0;
    Pt137(int px, int py) { x = px; y = py; }
    int manhattan(Pt137 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt137 other) { return x * other.x + y * other.y; }
}

class Worker137 {
    int total = 0;
    int count = 0;
    Mode137 mode = Mode137.Idle;
    string name = "worker137";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode137.Stop; } else { mode = Mode137.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner137 {
    int run() {
    auto rng = new Rng137(137);
    auto worker = new Worker137();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt137(1, 2);
    auto b = new Pt137(4, -6);
    int bias = 137;
    auto scale = n => n * bias;
    table t = { name = "m137", ["n"] = 137 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode138 {
    Idle,
    Run,
    Stop = 10
}

class Rng138 {
    int state = 0;
    Rng138(int seed) { state = seed + 138; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt138 {
    int x = 0;
    int y = 0;
    Pt138(int px, int py) { x = px; y = py; }
    int manhattan(Pt138 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt138 other) { return x * other.x + y * other.y; }
}

class Worker138 {
    int total = 0;
    int count = 0;
    Mode138 mode = Mode138.Idle;
    string name = "worker138";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode138.Stop; } else { mode = Mode138.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner138 {
    int run() {
    auto rng = new Rng138(138);
    auto worker = new Worker138();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt138(1, 2);
    auto b = new Pt138(4, -6);
    int bias = 138;
    auto scale = n => n * bias;
    table t = { name = "m138", ["n"] = 138 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode139 {
    Idle,
    Run,
    Stop = 10
}

class Rng139 {
    int state = 0;
    Rng139(int seed) { state = seed + 139; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt139 {
    int x = 0;
    int y = 0;
    Pt139(int px, int py) { x = px; y = py; }
    int manhattan(Pt139 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt139 other) { return x * other.x + y * other.y; }
}

class Worker139 {
    int total = 0;
    int count = 0;
    Mode139 mode = Mode139.Idle;
    string name = "worker139";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode139.Stop; } else { mode = Mode139.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner139 {
    int run() {
    auto rng = new Rng139(139);
    auto worker = new Worker139();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt139(1, 2);
    auto b = new Pt139(4, -6);
    int bias = 139;
    auto scale = n => n * bias;
    table t = { name = "m139", ["n"] = 139 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode140 {
    Idle,
    Run,
    Stop = 10
}

class Rng140 {
    int state = 0;
    Rng140(int seed) { state = seed + 140; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt140 {
    int x = 0;
    int y = 0;
    Pt140(int px, int py) { x = px; y = py; }
    int manhattan(Pt140 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt140 other) { return x * other.x + y * other.y; }
}

class Worker140 {
    int total = 0;
    int count = 0;
    Mode140 mode = Mode140.Idle;
    string name = "worker140";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode140.Stop; } else { mode = Mode140.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner140 {
    int run() {
    auto rng = new Rng140(140);
    auto worker = new Worker140();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt140(1, 2);
    auto b = new Pt140(4, -6);
    int bias = 140;
    auto scale = n => n * bias;
    table t = { name = "m140", ["n"] = 140 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode141 {
    Idle,
    Run,
    Stop = 10
}

class Rng141 {
    int state = 0;
    Rng141(int seed) { state = seed + 141; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt141 {
    int x = 0;
    int y = 0;
    Pt141(int px, int py) { x = px; y = py; }
    int manhattan(Pt141 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt141 other) { return x * other.x + y * other.y; }
}

class Worker141 {
    int total = 0;
    int count = 0;
    Mode141 mode = Mode141.Idle;
    string name = "worker141";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode141.Stop; } else { mode = Mode141.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner141 {
    int run() {
    auto rng = new Rng141(141);
    auto worker = new Worker141();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt141(1, 2);
    auto b = new Pt141(4, -6);
    int bias = 141;
    auto scale = n => n * bias;
    table t = { name = "m141", ["n"] = 141 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode142 {
    Idle,
    Run,
    Stop = 10
}

class Rng142 {
    int state = 0;
    Rng142(int seed) { state = seed + 142; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt142 {
    int x = 0;
    int y = 0;
    Pt142(int px, int py) { x = px; y = py; }
    int manhattan(Pt142 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt142 other) { return x * other.x + y * other.y; }
}

class Worker142 {
    int total = 0;
    int count = 0;
    Mode142 mode = Mode142.Idle;
    string name = "worker142";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode142.Stop; } else { mode = Mode142.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner142 {
    int run() {
    auto rng = new Rng142(142);
    auto worker = new Worker142();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt142(1, 2);
    auto b = new Pt142(4, -6);
    int bias = 142;
    auto scale = n => n * bias;
    table t = { name = "m142", ["n"] = 142 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode143 {
    Idle,
    Run,
    Stop = 10
}

class Rng143 {
    int state = 0;
    Rng143(int seed) { state = seed + 143; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt143 {
    int x = 0;
    int y = 0;
    Pt143(int px, int py) { x = px; y = py; }
    int manhattan(Pt143 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt143 other) { return x * other.x + y * other.y; }
}

class Worker143 {
    int total = 0;
    int count = 0;
    Mode143 mode = Mode143.Idle;
    string name = "worker143";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode143.Stop; } else { mode = Mode143.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner143 {
    int run() {
    auto rng = new Rng143(143);
    auto worker = new Worker143();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt143(1, 2);
    auto b = new Pt143(4, -6);
    int bias = 143;
    auto scale = n => n * bias;
    table t = { name = "m143", ["n"] = 143 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode144 {
    Idle,
    Run,
    Stop = 10
}

class Rng144 {
    int state = 0;
    Rng144(int seed) { state = seed + 144; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt144 {
    int x = 0;
    int y = 0;
    Pt144(int px, int py) { x = px; y = py; }
    int manhattan(Pt144 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt144 other) { return x * other.x + y * other.y; }
}

class Worker144 {
    int total = 0;
    int count = 0;
    Mode144 mode = Mode144.Idle;
    string name = "worker144";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode144.Stop; } else { mode = Mode144.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner144 {
    int run() {
    auto rng = new Rng144(144);
    auto worker = new Worker144();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt144(1, 2);
    auto b = new Pt144(4, -6);
    int bias = 144;
    auto scale = n => n * bias;
    table t = { name = "m144", ["n"] = 144 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode145 {
    Idle,
    Run,
    Stop = 10
}

class Rng145 {
    int state = 0;
    Rng145(int seed) { state = seed + 145; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt145 {
    int x = 0;
    int y = 0;
    Pt145(int px, int py) { x = px; y = py; }
    int manhattan(Pt145 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt145 other) { return x * other.x + y * other.y; }
}

class Worker145 {
    int total = 0;
    int count = 0;
    Mode145 mode = Mode145.Idle;
    string name = "worker145";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode145.Stop; } else { mode = Mode145.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner145 {
    int run() {
    auto rng = new Rng145(145);
    auto worker = new Worker145();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt145(1, 2);
    auto b = new Pt145(4, -6);
    int bias = 145;
    auto scale = n => n * bias;
    table t = { name = "m145", ["n"] = 145 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode146 {
    Idle,
    Run,
    Stop = 10
}

class Rng146 {
    int state = 0;
    Rng146(int seed) { state = seed + 146; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt146 {
    int x = 0;
    int y = 0;
    Pt146(int px, int py) { x = px; y = py; }
    int manhattan(Pt146 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt146 other) { return x * other.x + y * other.y; }
}

class Worker146 {
    int total = 0;
    int count = 0;
    Mode146 mode = Mode146.Idle;
    string name = "worker146";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode146.Stop; } else { mode = Mode146.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner146 {
    int run() {
    auto rng = new Rng146(146);
    auto worker = new Worker146();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt146(1, 2);
    auto b = new Pt146(4, -6);
    int bias = 146;
    auto scale = n => n * bias;
    table t = { name = "m146", ["n"] = 146 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode147 {
    Idle,
    Run,
    Stop = 10
}

class Rng147 {
    int state = 0;
    Rng147(int seed) { state = seed + 147; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt147 {
    int x = 0;
    int y = 0;
    Pt147(int px, int py) { x = px; y = py; }
    int manhattan(Pt147 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt147 other) { return x * other.x + y * other.y; }
}

class Worker147 {
    int total = 0;
    int count = 0;
    Mode147 mode = Mode147.Idle;
    string name = "worker147";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode147.Stop; } else { mode = Mode147.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner147 {
    int run() {
    auto rng = new Rng147(147);
    auto worker = new Worker147();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt147(1, 2);
    auto b = new Pt147(4, -6);
    int bias = 147;
    auto scale = n => n * bias;
    table t = { name = "m147", ["n"] = 147 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode148 {
    Idle,
    Run,
    Stop = 10
}

class Rng148 {
    int state = 0;
    Rng148(int seed) { state = seed + 148; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt148 {
    int x = 0;
    int y = 0;
    Pt148(int px, int py) { x = px; y = py; }
    int manhattan(Pt148 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt148 other) { return x * other.x + y * other.y; }
}

class Worker148 {
    int total = 0;
    int count = 0;
    Mode148 mode = Mode148.Idle;
    string name = "worker148";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode148.Stop; } else { mode = Mode148.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner148 {
    int run() {
    auto rng = new Rng148(148);
    auto worker = new Worker148();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt148(1, 2);
    auto b = new Pt148(4, -6);
    int bias = 148;
    auto scale = n => n * bias;
    table t = { name = "m148", ["n"] = 148 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode149 {
    Idle,
    Run,
    Stop = 10
}

class Rng149 {
    int state = 0;
    Rng149(int seed) { state = seed + 149; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt149 {
    int x = 0;
    int y = 0;
    Pt149(int px, int py) { x = px; y = py; }
    int manhattan(Pt149 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt149 other) { return x * other.x + y * other.y; }
}

class Worker149 {
    int total = 0;
    int count = 0;
    Mode149 mode = Mode149.Idle;
    string name = "worker149";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode149.Stop; } else { mode = Mode149.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner149 {
    int run() {
    auto rng = new Rng149(149);
    auto worker = new Worker149();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt149(1, 2);
    auto b = new Pt149(4, -6);
    int bias = 149;
    auto scale = n => n * bias;
    table t = { name = "m149", ["n"] = 149 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode150 {
    Idle,
    Run,
    Stop = 10
}

class Rng150 {
    int state = 0;
    Rng150(int seed) { state = seed + 150; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt150 {
    int x = 0;
    int y = 0;
    Pt150(int px, int py) { x = px; y = py; }
    int manhattan(Pt150 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt150 other) { return x * other.x + y * other.y; }
}

class Worker150 {
    int total = 0;
    int count = 0;
    Mode150 mode = Mode150.Idle;
    string name = "worker150";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode150.Stop; } else { mode = Mode150.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner150 {
    int run() {
    auto rng = new Rng150(150);
    auto worker = new Worker150();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt150(1, 2);
    auto b = new Pt150(4, -6);
    int bias = 150;
    auto scale = n => n * bias;
    table t = { name = "m150", ["n"] = 150 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode151 {
    Idle,
    Run,
    Stop = 10
}

class Rng151 {
    int state = 0;
    Rng151(int seed) { state = seed + 151; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt151 {
    int x = 0;
    int y = 0;
    Pt151(int px, int py) { x = px; y = py; }
    int manhattan(Pt151 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt151 other) { return x * other.x + y * other.y; }
}

class Worker151 {
    int total = 0;
    int count = 0;
    Mode151 mode = Mode151.Idle;
    string name = "worker151";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode151.Stop; } else { mode = Mode151.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner151 {
    int run() {
    auto rng = new Rng151(151);
    auto worker = new Worker151();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt151(1, 2);
    auto b = new Pt151(4, -6);
    int bias = 151;
    auto scale = n => n * bias;
    table t = { name = "m151", ["n"] = 151 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode152 {
    Idle,
    Run,
    Stop = 10
}

class Rng152 {
    int state = 0;
    Rng152(int seed) { state = seed + 152; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt152 {
    int x = 0;
    int y = 0;
    Pt152(int px, int py) { x = px; y = py; }
    int manhattan(Pt152 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt152 other) { return x * other.x + y * other.y; }
}

class Worker152 {
    int total = 0;
    int count = 0;
    Mode152 mode = Mode152.Idle;
    string name = "worker152";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode152.Stop; } else { mode = Mode152.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner152 {
    int run() {
    auto rng = new Rng152(152);
    auto worker = new Worker152();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt152(1, 2);
    auto b = new Pt152(4, -6);
    int bias = 152;
    auto scale = n => n * bias;
    table t = { name = "m152", ["n"] = 152 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode153 {
    Idle,
    Run,
    Stop = 10
}

class Rng153 {
    int state = 0;
    Rng153(int seed) { state = seed + 153; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt153 {
    int x = 0;
    int y = 0;
    Pt153(int px, int py) { x = px; y = py; }
    int manhattan(Pt153 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt153 other) { return x * other.x + y * other.y; }
}

class Worker153 {
    int total = 0;
    int count = 0;
    Mode153 mode = Mode153.Idle;
    string name = "worker153";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode153.Stop; } else { mode = Mode153.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner153 {
    int run() {
    auto rng = new Rng153(153);
    auto worker = new Worker153();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt153(1, 2);
    auto b = new Pt153(4, -6);
    int bias = 153;
    auto scale = n => n * bias;
    table t = { name = "m153", ["n"] = 153 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode154 {
    Idle,
    Run,
    Stop = 10
}

class Rng154 {
    int state = 0;
    Rng154(int seed) { state = seed + 154; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt154 {
    int x = 0;
    int y = 0;
    Pt154(int px, int py) { x = px; y = py; }
    int manhattan(Pt154 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt154 other) { return x * other.x + y * other.y; }
}

class Worker154 {
    int total = 0;
    int count = 0;
    Mode154 mode = Mode154.Idle;
    string name = "worker154";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode154.Stop; } else { mode = Mode154.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner154 {
    int run() {
    auto rng = new Rng154(154);
    auto worker = new Worker154();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt154(1, 2);
    auto b = new Pt154(4, -6);
    int bias = 154;
    auto scale = n => n * bias;
    table t = { name = "m154", ["n"] = 154 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode155 {
    Idle,
    Run,
    Stop = 10
}

class Rng155 {
    int state = 0;
    Rng155(int seed) { state = seed + 155; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt155 {
    int x = 0;
    int y = 0;
    Pt155(int px, int py) { x = px; y = py; }
    int manhattan(Pt155 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt155 other) { return x * other.x + y * other.y; }
}

class Worker155 {
    int total = 0;
    int count = 0;
    Mode155 mode = Mode155.Idle;
    string name = "worker155";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode155.Stop; } else { mode = Mode155.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner155 {
    int run() {
    auto rng = new Rng155(155);
    auto worker = new Worker155();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt155(1, 2);
    auto b = new Pt155(4, -6);
    int bias = 155;
    auto scale = n => n * bias;
    table t = { name = "m155", ["n"] = 155 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode156 {
    Idle,
    Run,
    Stop = 10
}

class Rng156 {
    int state = 0;
    Rng156(int seed) { state = seed + 156; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt156 {
    int x = 0;
    int y = 0;
    Pt156(int px, int py) { x = px; y = py; }
    int manhattan(Pt156 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt156 other) { return x * other.x + y * other.y; }
}

class Worker156 {
    int total = 0;
    int count = 0;
    Mode156 mode = Mode156.Idle;
    string name = "worker156";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode156.Stop; } else { mode = Mode156.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner156 {
    int run() {
    auto rng = new Rng156(156);
    auto worker = new Worker156();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt156(1, 2);
    auto b = new Pt156(4, -6);
    int bias = 156;
    auto scale = n => n * bias;
    table t = { name = "m156", ["n"] = 156 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode157 {
    Idle,
    Run,
    Stop = 10
}

class Rng157 {
    int state = 0;
    Rng157(int seed) { state = seed + 157; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt157 {
    int x = 0;
    int y = 0;
    Pt157(int px, int py) { x = px; y = py; }
    int manhattan(Pt157 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt157 other) { return x * other.x + y * other.y; }
}

class Worker157 {
    int total = 0;
    int count = 0;
    Mode157 mode = Mode157.Idle;
    string name = "worker157";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode157.Stop; } else { mode = Mode157.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner157 {
    int run() {
    auto rng = new Rng157(157);
    auto worker = new Worker157();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt157(1, 2);
    auto b = new Pt157(4, -6);
    int bias = 157;
    auto scale = n => n * bias;
    table t = { name = "m157", ["n"] = 157 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode158 {
    Idle,
    Run,
    Stop = 10
}

class Rng158 {
    int state = 0;
    Rng158(int seed) { state = seed + 158; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt158 {
    int x = 0;
    int y = 0;
    Pt158(int px, int py) { x = px; y = py; }
    int manhattan(Pt158 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt158 other) { return x * other.x + y * other.y; }
}

class Worker158 {
    int total = 0;
    int count = 0;
    Mode158 mode = Mode158.Idle;
    string name = "worker158";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode158.Stop; } else { mode = Mode158.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner158 {
    int run() {
    auto rng = new Rng158(158);
    auto worker = new Worker158();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt158(1, 2);
    auto b = new Pt158(4, -6);
    int bias = 158;
    auto scale = n => n * bias;
    table t = { name = "m158", ["n"] = 158 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode159 {
    Idle,
    Run,
    Stop = 10
}

class Rng159 {
    int state = 0;
    Rng159(int seed) { state = seed + 159; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt159 {
    int x = 0;
    int y = 0;
    Pt159(int px, int py) { x = px; y = py; }
    int manhattan(Pt159 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt159 other) { return x * other.x + y * other.y; }
}

class Worker159 {
    int total = 0;
    int count = 0;
    Mode159 mode = Mode159.Idle;
    string name = "worker159";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode159.Stop; } else { mode = Mode159.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner159 {
    int run() {
    auto rng = new Rng159(159);
    auto worker = new Worker159();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt159(1, 2);
    auto b = new Pt159(4, -6);
    int bias = 159;
    auto scale = n => n * bias;
    table t = { name = "m159", ["n"] = 159 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode160 {
    Idle,
    Run,
    Stop = 10
}

class Rng160 {
    int state = 0;
    Rng160(int seed) { state = seed + 160; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt160 {
    int x = 0;
    int y = 0;
    Pt160(int px, int py) { x = px; y = py; }
    int manhattan(Pt160 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt160 other) { return x * other.x + y * other.y; }
}

class Worker160 {
    int total = 0;
    int count = 0;
    Mode160 mode = Mode160.Idle;
    string name = "worker160";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode160.Stop; } else { mode = Mode160.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner160 {
    int run() {
    auto rng = new Rng160(160);
    auto worker = new Worker160();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt160(1, 2);
    auto b = new Pt160(4, -6);
    int bias = 160;
    auto scale = n => n * bias;
    table t = { name = "m160", ["n"] = 160 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode161 {
    Idle,
    Run,
    Stop = 10
}

class Rng161 {
    int state = 0;
    Rng161(int seed) { state = seed + 161; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt161 {
    int x = 0;
    int y = 0;
    Pt161(int px, int py) { x = px; y = py; }
    int manhattan(Pt161 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt161 other) { return x * other.x + y * other.y; }
}

class Worker161 {
    int total = 0;
    int count = 0;
    Mode161 mode = Mode161.Idle;
    string name = "worker161";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode161.Stop; } else { mode = Mode161.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner161 {
    int run() {
    auto rng = new Rng161(161);
    auto worker = new Worker161();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt161(1, 2);
    auto b = new Pt161(4, -6);
    int bias = 161;
    auto scale = n => n * bias;
    table t = { name = "m161", ["n"] = 161 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode162 {
    Idle,
    Run,
    Stop = 10
}

class Rng162 {
    int state = 0;
    Rng162(int seed) { state = seed + 162; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt162 {
    int x = 0;
    int y = 0;
    Pt162(int px, int py) { x = px; y = py; }
    int manhattan(Pt162 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt162 other) { return x * other.x + y * other.y; }
}

class Worker162 {
    int total = 0;
    int count = 0;
    Mode162 mode = Mode162.Idle;
    string name = "worker162";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode162.Stop; } else { mode = Mode162.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner162 {
    int run() {
    auto rng = new Rng162(162);
    auto worker = new Worker162();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt162(1, 2);
    auto b = new Pt162(4, -6);
    int bias = 162;
    auto scale = n => n * bias;
    table t = { name = "m162", ["n"] = 162 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode163 {
    Idle,
    Run,
    Stop = 10
}

class Rng163 {
    int state = 0;
    Rng163(int seed) { state = seed + 163; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt163 {
    int x = 0;
    int y = 0;
    Pt163(int px, int py) { x = px; y = py; }
    int manhattan(Pt163 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt163 other) { return x * other.x + y * other.y; }
}

class Worker163 {
    int total = 0;
    int count = 0;
    Mode163 mode = Mode163.Idle;
    string name = "worker163";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode163.Stop; } else { mode = Mode163.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner163 {
    int run() {
    auto rng = new Rng163(163);
    auto worker = new Worker163();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt163(1, 2);
    auto b = new Pt163(4, -6);
    int bias = 163;
    auto scale = n => n * bias;
    table t = { name = "m163", ["n"] = 163 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode164 {
    Idle,
    Run,
    Stop = 10
}

class Rng164 {
    int state = 0;
    Rng164(int seed) { state = seed + 164; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt164 {
    int x = 0;
    int y = 0;
    Pt164(int px, int py) { x = px; y = py; }
    int manhattan(Pt164 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt164 other) { return x * other.x + y * other.y; }
}

class Worker164 {
    int total = 0;
    int count = 0;
    Mode164 mode = Mode164.Idle;
    string name = "worker164";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode164.Stop; } else { mode = Mode164.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner164 {
    int run() {
    auto rng = new Rng164(164);
    auto worker = new Worker164();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt164(1, 2);
    auto b = new Pt164(4, -6);
    int bias = 164;
    auto scale = n => n * bias;
    table t = { name = "m164", ["n"] = 164 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode165 {
    Idle,
    Run,
    Stop = 10
}

class Rng165 {
    int state = 0;
    Rng165(int seed) { state = seed + 165; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt165 {
    int x = 0;
    int y = 0;
    Pt165(int px, int py) { x = px; y = py; }
    int manhattan(Pt165 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt165 other) { return x * other.x + y * other.y; }
}

class Worker165 {
    int total = 0;
    int count = 0;
    Mode165 mode = Mode165.Idle;
    string name = "worker165";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode165.Stop; } else { mode = Mode165.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner165 {
    int run() {
    auto rng = new Rng165(165);
    auto worker = new Worker165();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt165(1, 2);
    auto b = new Pt165(4, -6);
    int bias = 165;
    auto scale = n => n * bias;
    table t = { name = "m165", ["n"] = 165 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode166 {
    Idle,
    Run,
    Stop = 10
}

class Rng166 {
    int state = 0;
    Rng166(int seed) { state = seed + 166; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt166 {
    int x = 0;
    int y = 0;
    Pt166(int px, int py) { x = px; y = py; }
    int manhattan(Pt166 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt166 other) { return x * other.x + y * other.y; }
}

class Worker166 {
    int total = 0;
    int count = 0;
    Mode166 mode = Mode166.Idle;
    string name = "worker166";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode166.Stop; } else { mode = Mode166.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner166 {
    int run() {
    auto rng = new Rng166(166);
    auto worker = new Worker166();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt166(1, 2);
    auto b = new Pt166(4, -6);
    int bias = 166;
    auto scale = n => n * bias;
    table t = { name = "m166", ["n"] = 166 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode167 {
    Idle,
    Run,
    Stop = 10
}

class Rng167 {
    int state = 0;
    Rng167(int seed) { state = seed + 167; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt167 {
    int x = 0;
    int y = 0;
    Pt167(int px, int py) { x = px; y = py; }
    int manhattan(Pt167 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt167 other) { return x * other.x + y * other.y; }
}

class Worker167 {
    int total = 0;
    int count = 0;
    Mode167 mode = Mode167.Idle;
    string name = "worker167";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode167.Stop; } else { mode = Mode167.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner167 {
    int run() {
    auto rng = new Rng167(167);
    auto worker = new Worker167();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt167(1, 2);
    auto b = new Pt167(4, -6);
    int bias = 167;
    auto scale = n => n * bias;
    table t = { name = "m167", ["n"] = 167 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode168 {
    Idle,
    Run,
    Stop = 10
}

class Rng168 {
    int state = 0;
    Rng168(int seed) { state = seed + 168; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt168 {
    int x = 0;
    int y = 0;
    Pt168(int px, int py) { x = px; y = py; }
    int manhattan(Pt168 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt168 other) { return x * other.x + y * other.y; }
}

class Worker168 {
    int total = 0;
    int count = 0;
    Mode168 mode = Mode168.Idle;
    string name = "worker168";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode168.Stop; } else { mode = Mode168.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner168 {
    int run() {
    auto rng = new Rng168(168);
    auto worker = new Worker168();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt168(1, 2);
    auto b = new Pt168(4, -6);
    int bias = 168;
    auto scale = n => n * bias;
    table t = { name = "m168", ["n"] = 168 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode169 {
    Idle,
    Run,
    Stop = 10
}

class Rng169 {
    int state = 0;
    Rng169(int seed) { state = seed + 169; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt169 {
    int x = 0;
    int y = 0;
    Pt169(int px, int py) { x = px; y = py; }
    int manhattan(Pt169 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt169 other) { return x * other.x + y * other.y; }
}

class Worker169 {
    int total = 0;
    int count = 0;
    Mode169 mode = Mode169.Idle;
    string name = "worker169";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode169.Stop; } else { mode = Mode169.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner169 {
    int run() {
    auto rng = new Rng169(169);
    auto worker = new Worker169();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt169(1, 2);
    auto b = new Pt169(4, -6);
    int bias = 169;
    auto scale = n => n * bias;
    table t = { name = "m169", ["n"] = 169 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode170 {
    Idle,
    Run,
    Stop = 10
}

class Rng170 {
    int state = 0;
    Rng170(int seed) { state = seed + 170; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt170 {
    int x = 0;
    int y = 0;
    Pt170(int px, int py) { x = px; y = py; }
    int manhattan(Pt170 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt170 other) { return x * other.x + y * other.y; }
}

class Worker170 {
    int total = 0;
    int count = 0;
    Mode170 mode = Mode170.Idle;
    string name = "worker170";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode170.Stop; } else { mode = Mode170.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner170 {
    int run() {
    auto rng = new Rng170(170);
    auto worker = new Worker170();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt170(1, 2);
    auto b = new Pt170(4, -6);
    int bias = 170;
    auto scale = n => n * bias;
    table t = { name = "m170", ["n"] = 170 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode171 {
    Idle,
    Run,
    Stop = 10
}

class Rng171 {
    int state = 0;
    Rng171(int seed) { state = seed + 171; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt171 {
    int x = 0;
    int y = 0;
    Pt171(int px, int py) { x = px; y = py; }
    int manhattan(Pt171 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt171 other) { return x * other.x + y * other.y; }
}

class Worker171 {
    int total = 0;
    int count = 0;
    Mode171 mode = Mode171.Idle;
    string name = "worker171";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode171.Stop; } else { mode = Mode171.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner171 {
    int run() {
    auto rng = new Rng171(171);
    auto worker = new Worker171();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt171(1, 2);
    auto b = new Pt171(4, -6);
    int bias = 171;
    auto scale = n => n * bias;
    table t = { name = "m171", ["n"] = 171 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode172 {
    Idle,
    Run,
    Stop = 10
}

class Rng172 {
    int state = 0;
    Rng172(int seed) { state = seed + 172; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt172 {
    int x = 0;
    int y = 0;
    Pt172(int px, int py) { x = px; y = py; }
    int manhattan(Pt172 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt172 other) { return x * other.x + y * other.y; }
}

class Worker172 {
    int total = 0;
    int count = 0;
    Mode172 mode = Mode172.Idle;
    string name = "worker172";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode172.Stop; } else { mode = Mode172.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner172 {
    int run() {
    auto rng = new Rng172(172);
    auto worker = new Worker172();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt172(1, 2);
    auto b = new Pt172(4, -6);
    int bias = 172;
    auto scale = n => n * bias;
    table t = { name = "m172", ["n"] = 172 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode173 {
    Idle,
    Run,
    Stop = 10
}

class Rng173 {
    int state = 0;
    Rng173(int seed) { state = seed + 173; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt173 {
    int x = 0;
    int y = 0;
    Pt173(int px, int py) { x = px; y = py; }
    int manhattan(Pt173 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt173 other) { return x * other.x + y * other.y; }
}

class Worker173 {
    int total = 0;
    int count = 0;
    Mode173 mode = Mode173.Idle;
    string name = "worker173";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode173.Stop; } else { mode = Mode173.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner173 {
    int run() {
    auto rng = new Rng173(173);
    auto worker = new Worker173();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt173(1, 2);
    auto b = new Pt173(4, -6);
    int bias = 173;
    auto scale = n => n * bias;
    table t = { name = "m173", ["n"] = 173 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode174 {
    Idle,
    Run,
    Stop = 10
}

class Rng174 {
    int state = 0;
    Rng174(int seed) { state = seed + 174; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt174 {
    int x = 0;
    int y = 0;
    Pt174(int px, int py) { x = px; y = py; }
    int manhattan(Pt174 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt174 other) { return x * other.x + y * other.y; }
}

class Worker174 {
    int total = 0;
    int count = 0;
    Mode174 mode = Mode174.Idle;
    string name = "worker174";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode174.Stop; } else { mode = Mode174.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner174 {
    int run() {
    auto rng = new Rng174(174);
    auto worker = new Worker174();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt174(1, 2);
    auto b = new Pt174(4, -6);
    int bias = 174;
    auto scale = n => n * bias;
    table t = { name = "m174", ["n"] = 174 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode175 {
    Idle,
    Run,
    Stop = 10
}

class Rng175 {
    int state = 0;
    Rng175(int seed) { state = seed + 175; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt175 {
    int x = 0;
    int y = 0;
    Pt175(int px, int py) { x = px; y = py; }
    int manhattan(Pt175 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt175 other) { return x * other.x + y * other.y; }
}

class Worker175 {
    int total = 0;
    int count = 0;
    Mode175 mode = Mode175.Idle;
    string name = "worker175";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode175.Stop; } else { mode = Mode175.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner175 {
    int run() {
    auto rng = new Rng175(175);
    auto worker = new Worker175();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt175(1, 2);
    auto b = new Pt175(4, -6);
    int bias = 175;
    auto scale = n => n * bias;
    table t = { name = "m175", ["n"] = 175 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode176 {
    Idle,
    Run,
    Stop = 10
}

class Rng176 {
    int state = 0;
    Rng176(int seed) { state = seed + 176; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt176 {
    int x = 0;
    int y = 0;
    Pt176(int px, int py) { x = px; y = py; }
    int manhattan(Pt176 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt176 other) { return x * other.x + y * other.y; }
}

class Worker176 {
    int total = 0;
    int count = 0;
    Mode176 mode = Mode176.Idle;
    string name = "worker176";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode176.Stop; } else { mode = Mode176.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner176 {
    int run() {
    auto rng = new Rng176(176);
    auto worker = new Worker176();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt176(1, 2);
    auto b = new Pt176(4, -6);
    int bias = 176;
    auto scale = n => n * bias;
    table t = { name = "m176", ["n"] = 176 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode177 {
    Idle,
    Run,
    Stop = 10
}

class Rng177 {
    int state = 0;
    Rng177(int seed) { state = seed + 177; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt177 {
    int x = 0;
    int y = 0;
    Pt177(int px, int py) { x = px; y = py; }
    int manhattan(Pt177 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt177 other) { return x * other.x + y * other.y; }
}

class Worker177 {
    int total = 0;
    int count = 0;
    Mode177 mode = Mode177.Idle;
    string name = "worker177";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode177.Stop; } else { mode = Mode177.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner177 {
    int run() {
    auto rng = new Rng177(177);
    auto worker = new Worker177();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt177(1, 2);
    auto b = new Pt177(4, -6);
    int bias = 177;
    auto scale = n => n * bias;
    table t = { name = "m177", ["n"] = 177 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode178 {
    Idle,
    Run,
    Stop = 10
}

class Rng178 {
    int state = 0;
    Rng178(int seed) { state = seed + 178; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt178 {
    int x = 0;
    int y = 0;
    Pt178(int px, int py) { x = px; y = py; }
    int manhattan(Pt178 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt178 other) { return x * other.x + y * other.y; }
}

class Worker178 {
    int total = 0;
    int count = 0;
    Mode178 mode = Mode178.Idle;
    string name = "worker178";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode178.Stop; } else { mode = Mode178.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner178 {
    int run() {
    auto rng = new Rng178(178);
    auto worker = new Worker178();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt178(1, 2);
    auto b = new Pt178(4, -6);
    int bias = 178;
    auto scale = n => n * bias;
    table t = { name = "m178", ["n"] = 178 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode179 {
    Idle,
    Run,
    Stop = 10
}

class Rng179 {
    int state = 0;
    Rng179(int seed) { state = seed + 179; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt179 {
    int x = 0;
    int y = 0;
    Pt179(int px, int py) { x = px; y = py; }
    int manhattan(Pt179 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt179 other) { return x * other.x + y * other.y; }
}

class Worker179 {
    int total = 0;
    int count = 0;
    Mode179 mode = Mode179.Idle;
    string name = "worker179";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode179.Stop; } else { mode = Mode179.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner179 {
    int run() {
    auto rng = new Rng179(179);
    auto worker = new Worker179();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt179(1, 2);
    auto b = new Pt179(4, -6);
    int bias = 179;
    auto scale = n => n * bias;
    table t = { name = "m179", ["n"] = 179 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode180 {
    Idle,
    Run,
    Stop = 10
}

class Rng180 {
    int state = 0;
    Rng180(int seed) { state = seed + 180; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt180 {
    int x = 0;
    int y = 0;
    Pt180(int px, int py) { x = px; y = py; }
    int manhattan(Pt180 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt180 other) { return x * other.x + y * other.y; }
}

class Worker180 {
    int total = 0;
    int count = 0;
    Mode180 mode = Mode180.Idle;
    string name = "worker180";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode180.Stop; } else { mode = Mode180.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner180 {
    int run() {
    auto rng = new Rng180(180);
    auto worker = new Worker180();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt180(1, 2);
    auto b = new Pt180(4, -6);
    int bias = 180;
    auto scale = n => n * bias;
    table t = { name = "m180", ["n"] = 180 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode181 {
    Idle,
    Run,
    Stop = 10
}

class Rng181 {
    int state = 0;
    Rng181(int seed) { state = seed + 181; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt181 {
    int x = 0;
    int y = 0;
    Pt181(int px, int py) { x = px; y = py; }
    int manhattan(Pt181 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt181 other) { return x * other.x + y * other.y; }
}

class Worker181 {
    int total = 0;
    int count = 0;
    Mode181 mode = Mode181.Idle;
    string name = "worker181";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode181.Stop; } else { mode = Mode181.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner181 {
    int run() {
    auto rng = new Rng181(181);
    auto worker = new Worker181();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt181(1, 2);
    auto b = new Pt181(4, -6);
    int bias = 181;
    auto scale = n => n * bias;
    table t = { name = "m181", ["n"] = 181 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode182 {
    Idle,
    Run,
    Stop = 10
}

class Rng182 {
    int state = 0;
    Rng182(int seed) { state = seed + 182; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt182 {
    int x = 0;
    int y = 0;
    Pt182(int px, int py) { x = px; y = py; }
    int manhattan(Pt182 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt182 other) { return x * other.x + y * other.y; }
}

class Worker182 {
    int total = 0;
    int count = 0;
    Mode182 mode = Mode182.Idle;
    string name = "worker182";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode182.Stop; } else { mode = Mode182.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner182 {
    int run() {
    auto rng = new Rng182(182);
    auto worker = new Worker182();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt182(1, 2);
    auto b = new Pt182(4, -6);
    int bias = 182;
    auto scale = n => n * bias;
    table t = { name = "m182", ["n"] = 182 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode183 {
    Idle,
    Run,
    Stop = 10
}

class Rng183 {
    int state = 0;
    Rng183(int seed) { state = seed + 183; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt183 {
    int x = 0;
    int y = 0;
    Pt183(int px, int py) { x = px; y = py; }
    int manhattan(Pt183 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt183 other) { return x * other.x + y * other.y; }
}

class Worker183 {
    int total = 0;
    int count = 0;
    Mode183 mode = Mode183.Idle;
    string name = "worker183";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode183.Stop; } else { mode = Mode183.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner183 {
    int run() {
    auto rng = new Rng183(183);
    auto worker = new Worker183();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt183(1, 2);
    auto b = new Pt183(4, -6);
    int bias = 183;
    auto scale = n => n * bias;
    table t = { name = "m183", ["n"] = 183 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode184 {
    Idle,
    Run,
    Stop = 10
}

class Rng184 {
    int state = 0;
    Rng184(int seed) { state = seed + 184; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt184 {
    int x = 0;
    int y = 0;
    Pt184(int px, int py) { x = px; y = py; }
    int manhattan(Pt184 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt184 other) { return x * other.x + y * other.y; }
}

class Worker184 {
    int total = 0;
    int count = 0;
    Mode184 mode = Mode184.Idle;
    string name = "worker184";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode184.Stop; } else { mode = Mode184.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner184 {
    int run() {
    auto rng = new Rng184(184);
    auto worker = new Worker184();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt184(1, 2);
    auto b = new Pt184(4, -6);
    int bias = 184;
    auto scale = n => n * bias;
    table t = { name = "m184", ["n"] = 184 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode185 {
    Idle,
    Run,
    Stop = 10
}

class Rng185 {
    int state = 0;
    Rng185(int seed) { state = seed + 185; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt185 {
    int x = 0;
    int y = 0;
    Pt185(int px, int py) { x = px; y = py; }
    int manhattan(Pt185 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt185 other) { return x * other.x + y * other.y; }
}

class Worker185 {
    int total = 0;
    int count = 0;
    Mode185 mode = Mode185.Idle;
    string name = "worker185";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode185.Stop; } else { mode = Mode185.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner185 {
    int run() {
    auto rng = new Rng185(185);
    auto worker = new Worker185();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt185(1, 2);
    auto b = new Pt185(4, -6);
    int bias = 185;
    auto scale = n => n * bias;
    table t = { name = "m185", ["n"] = 185 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode186 {
    Idle,
    Run,
    Stop = 10
}

class Rng186 {
    int state = 0;
    Rng186(int seed) { state = seed + 186; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt186 {
    int x = 0;
    int y = 0;
    Pt186(int px, int py) { x = px; y = py; }
    int manhattan(Pt186 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt186 other) { return x * other.x + y * other.y; }
}

class Worker186 {
    int total = 0;
    int count = 0;
    Mode186 mode = Mode186.Idle;
    string name = "worker186";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode186.Stop; } else { mode = Mode186.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner186 {
    int run() {
    auto rng = new Rng186(186);
    auto worker = new Worker186();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt186(1, 2);
    auto b = new Pt186(4, -6);
    int bias = 186;
    auto scale = n => n * bias;
    table t = { name = "m186", ["n"] = 186 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode187 {
    Idle,
    Run,
    Stop = 10
}

class Rng187 {
    int state = 0;
    Rng187(int seed) { state = seed + 187; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt187 {
    int x = 0;
    int y = 0;
    Pt187(int px, int py) { x = px; y = py; }
    int manhattan(Pt187 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt187 other) { return x * other.x + y * other.y; }
}

class Worker187 {
    int total = 0;
    int count = 0;
    Mode187 mode = Mode187.Idle;
    string name = "worker187";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode187.Stop; } else { mode = Mode187.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner187 {
    int run() {
    auto rng = new Rng187(187);
    auto worker = new Worker187();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt187(1, 2);
    auto b = new Pt187(4, -6);
    int bias = 187;
    auto scale = n => n * bias;
    table t = { name = "m187", ["n"] = 187 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode188 {
    Idle,
    Run,
    Stop = 10
}

class Rng188 {
    int state = 0;
    Rng188(int seed) { state = seed + 188; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt188 {
    int x = 0;
    int y = 0;
    Pt188(int px, int py) { x = px; y = py; }
    int manhattan(Pt188 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt188 other) { return x * other.x + y * other.y; }
}

class Worker188 {
    int total = 0;
    int count = 0;
    Mode188 mode = Mode188.Idle;
    string name = "worker188";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode188.Stop; } else { mode = Mode188.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner188 {
    int run() {
    auto rng = new Rng188(188);
    auto worker = new Worker188();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt188(1, 2);
    auto b = new Pt188(4, -6);
    int bias = 188;
    auto scale = n => n * bias;
    table t = { name = "m188", ["n"] = 188 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode189 {
    Idle,
    Run,
    Stop = 10
}

class Rng189 {
    int state = 0;
    Rng189(int seed) { state = seed + 189; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt189 {
    int x = 0;
    int y = 0;
    Pt189(int px, int py) { x = px; y = py; }
    int manhattan(Pt189 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt189 other) { return x * other.x + y * other.y; }
}

class Worker189 {
    int total = 0;
    int count = 0;
    Mode189 mode = Mode189.Idle;
    string name = "worker189";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode189.Stop; } else { mode = Mode189.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner189 {
    int run() {
    auto rng = new Rng189(189);
    auto worker = new Worker189();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt189(1, 2);
    auto b = new Pt189(4, -6);
    int bias = 189;
    auto scale = n => n * bias;
    table t = { name = "m189", ["n"] = 189 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode190 {
    Idle,
    Run,
    Stop = 10
}

class Rng190 {
    int state = 0;
    Rng190(int seed) { state = seed + 190; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt190 {
    int x = 0;
    int y = 0;
    Pt190(int px, int py) { x = px; y = py; }
    int manhattan(Pt190 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt190 other) { return x * other.x + y * other.y; }
}

class Worker190 {
    int total = 0;
    int count = 0;
    Mode190 mode = Mode190.Idle;
    string name = "worker190";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode190.Stop; } else { mode = Mode190.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner190 {
    int run() {
    auto rng = new Rng190(190);
    auto worker = new Worker190();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt190(1, 2);
    auto b = new Pt190(4, -6);
    int bias = 190;
    auto scale = n => n * bias;
    table t = { name = "m190", ["n"] = 190 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode191 {
    Idle,
    Run,
    Stop = 10
}

class Rng191 {
    int state = 0;
    Rng191(int seed) { state = seed + 191; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt191 {
    int x = 0;
    int y = 0;
    Pt191(int px, int py) { x = px; y = py; }
    int manhattan(Pt191 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt191 other) { return x * other.x + y * other.y; }
}

class Worker191 {
    int total = 0;
    int count = 0;
    Mode191 mode = Mode191.Idle;
    string name = "worker191";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode191.Stop; } else { mode = Mode191.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner191 {
    int run() {
    auto rng = new Rng191(191);
    auto worker = new Worker191();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt191(1, 2);
    auto b = new Pt191(4, -6);
    int bias = 191;
    auto scale = n => n * bias;
    table t = { name = "m191", ["n"] = 191 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode192 {
    Idle,
    Run,
    Stop = 10
}

class Rng192 {
    int state = 0;
    Rng192(int seed) { state = seed + 192; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt192 {
    int x = 0;
    int y = 0;
    Pt192(int px, int py) { x = px; y = py; }
    int manhattan(Pt192 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt192 other) { return x * other.x + y * other.y; }
}

class Worker192 {
    int total = 0;
    int count = 0;
    Mode192 mode = Mode192.Idle;
    string name = "worker192";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode192.Stop; } else { mode = Mode192.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner192 {
    int run() {
    auto rng = new Rng192(192);
    auto worker = new Worker192();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt192(1, 2);
    auto b = new Pt192(4, -6);
    int bias = 192;
    auto scale = n => n * bias;
    table t = { name = "m192", ["n"] = 192 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode193 {
    Idle,
    Run,
    Stop = 10
}

class Rng193 {
    int state = 0;
    Rng193(int seed) { state = seed + 193; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt193 {
    int x = 0;
    int y = 0;
    Pt193(int px, int py) { x = px; y = py; }
    int manhattan(Pt193 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt193 other) { return x * other.x + y * other.y; }
}

class Worker193 {
    int total = 0;
    int count = 0;
    Mode193 mode = Mode193.Idle;
    string name = "worker193";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode193.Stop; } else { mode = Mode193.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner193 {
    int run() {
    auto rng = new Rng193(193);
    auto worker = new Worker193();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt193(1, 2);
    auto b = new Pt193(4, -6);
    int bias = 193;
    auto scale = n => n * bias;
    table t = { name = "m193", ["n"] = 193 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode194 {
    Idle,
    Run,
    Stop = 10
}

class Rng194 {
    int state = 0;
    Rng194(int seed) { state = seed + 194; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt194 {
    int x = 0;
    int y = 0;
    Pt194(int px, int py) { x = px; y = py; }
    int manhattan(Pt194 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt194 other) { return x * other.x + y * other.y; }
}

class Worker194 {
    int total = 0;
    int count = 0;
    Mode194 mode = Mode194.Idle;
    string name = "worker194";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode194.Stop; } else { mode = Mode194.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner194 {
    int run() {
    auto rng = new Rng194(194);
    auto worker = new Worker194();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt194(1, 2);
    auto b = new Pt194(4, -6);
    int bias = 194;
    auto scale = n => n * bias;
    table t = { name = "m194", ["n"] = 194 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode195 {
    Idle,
    Run,
    Stop = 10
}

class Rng195 {
    int state = 0;
    Rng195(int seed) { state = seed + 195; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt195 {
    int x = 0;
    int y = 0;
    Pt195(int px, int py) { x = px; y = py; }
    int manhattan(Pt195 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt195 other) { return x * other.x + y * other.y; }
}

class Worker195 {
    int total = 0;
    int count = 0;
    Mode195 mode = Mode195.Idle;
    string name = "worker195";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode195.Stop; } else { mode = Mode195.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner195 {
    int run() {
    auto rng = new Rng195(195);
    auto worker = new Worker195();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt195(1, 2);
    auto b = new Pt195(4, -6);
    int bias = 195;
    auto scale = n => n * bias;
    table t = { name = "m195", ["n"] = 195 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode196 {
    Idle,
    Run,
    Stop = 10
}

class Rng196 {
    int state = 0;
    Rng196(int seed) { state = seed + 196; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt196 {
    int x = 0;
    int y = 0;
    Pt196(int px, int py) { x = px; y = py; }
    int manhattan(Pt196 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt196 other) { return x * other.x + y * other.y; }
}

class Worker196 {
    int total = 0;
    int count = 0;
    Mode196 mode = Mode196.Idle;
    string name = "worker196";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode196.Stop; } else { mode = Mode196.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner196 {
    int run() {
    auto rng = new Rng196(196);
    auto worker = new Worker196();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt196(1, 2);
    auto b = new Pt196(4, -6);
    int bias = 196;
    auto scale = n => n * bias;
    table t = { name = "m196", ["n"] = 196 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode197 {
    Idle,
    Run,
    Stop = 10
}

class Rng197 {
    int state = 0;
    Rng197(int seed) { state = seed + 197; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt197 {
    int x = 0;
    int y = 0;
    Pt197(int px, int py) { x = px; y = py; }
    int manhattan(Pt197 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt197 other) { return x * other.x + y * other.y; }
}

class Worker197 {
    int total = 0;
    int count = 0;
    Mode197 mode = Mode197.Idle;
    string name = "worker197";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode197.Stop; } else { mode = Mode197.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner197 {
    int run() {
    auto rng = new Rng197(197);
    auto worker = new Worker197();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt197(1, 2);
    auto b = new Pt197(4, -6);
    int bias = 197;
    auto scale = n => n * bias;
    table t = { name = "m197", ["n"] = 197 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode198 {
    Idle,
    Run,
    Stop = 10
}

class Rng198 {
    int state = 0;
    Rng198(int seed) { state = seed + 198; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt198 {
    int x = 0;
    int y = 0;
    Pt198(int px, int py) { x = px; y = py; }
    int manhattan(Pt198 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt198 other) { return x * other.x + y * other.y; }
}

class Worker198 {
    int total = 0;
    int count = 0;
    Mode198 mode = Mode198.Idle;
    string name = "worker198";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode198.Stop; } else { mode = Mode198.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner198 {
    int run() {
    auto rng = new Rng198(198);
    auto worker = new Worker198();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt198(1, 2);
    auto b = new Pt198(4, -6);
    int bias = 198;
    auto scale = n => n * bias;
    table t = { name = "m198", ["n"] = 198 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}

enum Mode199 {
    Idle,
    Run,
    Stop = 10
}

class Rng199 {
    int state = 0;
    Rng199(int seed) { state = seed + 199; }
    int next() {
        state = (state * 1103515245 + 12345) % 2147483648;
        if (state < 0) { state = -state; }
        return state;
    }
    int range(int low, int high) { return low + next() % (high - low + 1); }
}

class Pt199 {
    int x = 0;
    int y = 0;
    Pt199(int px, int py) { x = px; y = py; }
    int manhattan(Pt199 other) {
        int dx = x - other.x;
        int dy = y - other.y;
        if (dx < 0) { dx = -dx; }
        if (dy < 0) { dy = -dy; }
        return dx + dy;
    }
    int dot(Pt199 other) { return x * other.x + y * other.y; }
}

class Worker199 {
    int total = 0;
    int count = 0;
    Mode199 mode = Mode199.Idle;
    string name = "worker199";

    void add(int sample) {
        count += 1;
        total += sample;
        if (total > 1000) { mode = Mode199.Stop; } else { mode = Mode199.Run; }
    }

    float mean() {
        if (count == 0) { return 0.0; }
        return total / count;
    }

    int primes(int limit) {
        int found = 0;
        for (int n = 2; n < limit; n += 1) {
            bool prime = true;
            for (int d = 2; d * d <= n; d += 1) {
                if (n % d == 0) { prime = false; break; }
            }
            if (!prime) { continue; }
            found += 1;
        }
        return found;
    }

    int fib(int n) {
        int a = 0;
        int b = 1;
        int i = 0;
        while (i < n) {
            int next = a + b;
            a = b;
            b = next;
            i += 1;
        }
        return a;
    }

    int text(string word) {
        int result = word.length();
        if (word.starts_with("wor")) { result += 10; }
        if (word.contains("ker")) { result += 20; }
        return result;
    }
}

class Runner199 {
    int run() {
    auto rng = new Rng199(199);
    auto worker = new Worker199();
    for (int k = 0; k < 4; k += 1) { worker.add(rng.range(1, 100)); }
    auto a = new Pt199(1, 2);
    auto b = new Pt199(4, -6);
    int bias = 199;
    auto scale = n => n * bias;
    table t = { name = "m199", ["n"] = 199 };
    t["done"] = true;
    scale(2);
    return a.manhattan(b) + a.dot(b) + worker.primes(30) + worker.fib(10) + worker.text(worker.name);
}
}
int checksum = 0;
checksum = (checksum + new Runner0().run()) % 1000003;
checksum = (checksum + new Runner1().run()) % 1000003;
checksum = (checksum + new Runner2().run()) % 1000003;
checksum = (checksum + new Runner3().run()) % 1000003;
checksum = (checksum + new Runner4().run()) % 1000003;
checksum = (checksum + new Runner5().run()) % 1000003;
checksum = (checksum + new Runner6().run()) % 1000003;
checksum = (checksum + new Runner7().run()) % 1000003;
checksum = (checksum + new Runner8().run()) % 1000003;
checksum = (checksum + new Runner9().run()) % 1000003;
checksum = (checksum + new Runner10().run()) % 1000003;
checksum = (checksum + new Runner11().run()) % 1000003;
checksum = (checksum + new Runner12().run()) % 1000003;
checksum = (checksum + new Runner13().run()) % 1000003;
checksum = (checksum + new Runner14().run()) % 1000003;
checksum = (checksum + new Runner15().run()) % 1000003;
checksum = (checksum + new Runner16().run()) % 1000003;
checksum = (checksum + new Runner17().run()) % 1000003;
checksum = (checksum + new Runner18().run()) % 1000003;
checksum = (checksum + new Runner19().run()) % 1000003;
checksum = (checksum + new Runner20().run()) % 1000003;
checksum = (checksum + new Runner21().run()) % 1000003;
checksum = (checksum + new Runner22().run()) % 1000003;
checksum = (checksum + new Runner23().run()) % 1000003;
checksum = (checksum + new Runner24().run()) % 1000003;
checksum = (checksum + new Runner25().run()) % 1000003;
checksum = (checksum + new Runner26().run()) % 1000003;
checksum = (checksum + new Runner27().run()) % 1000003;
checksum = (checksum + new Runner28().run()) % 1000003;
checksum = (checksum + new Runner29().run()) % 1000003;
checksum = (checksum + new Runner30().run()) % 1000003;
checksum = (checksum + new Runner31().run()) % 1000003;
checksum = (checksum + new Runner32().run()) % 1000003;
checksum = (checksum + new Runner33().run()) % 1000003;
checksum = (checksum + new Runner34().run()) % 1000003;
checksum = (checksum + new Runner35().run()) % 1000003;
checksum = (checksum + new Runner36().run()) % 1000003;
checksum = (checksum + new Runner37().run()) % 1000003;
checksum = (checksum + new Runner38().run()) % 1000003;
checksum = (checksum + new Runner39().run()) % 1000003;
checksum = (checksum + new Runner40().run()) % 1000003;
checksum = (checksum + new Runner41().run()) % 1000003;
checksum = (checksum + new Runner42().run()) % 1000003;
checksum = (checksum + new Runner43().run()) % 1000003;
checksum = (checksum + new Runner44().run()) % 1000003;
checksum = (checksum + new Runner45().run()) % 1000003;
checksum = (checksum + new Runner46().run()) % 1000003;
checksum = (checksum + new Runner47().run()) % 1000003;
checksum = (checksum + new Runner48().run()) % 1000003;
checksum = (checksum + new Runner49().run()) % 1000003;
checksum = (checksum + new Runner50().run()) % 1000003;
checksum = (checksum + new Runner51().run()) % 1000003;
checksum = (checksum + new Runner52().run()) % 1000003;
checksum = (checksum + new Runner53().run()) % 1000003;
checksum = (checksum + new Runner54().run()) % 1000003;
checksum = (checksum + new Runner55().run()) % 1000003;
checksum = (checksum + new Runner56().run()) % 1000003;
checksum = (checksum + new Runner57().run()) % 1000003;
checksum = (checksum + new Runner58().run()) % 1000003;
checksum = (checksum + new Runner59().run()) % 1000003;
checksum = (checksum + new Runner60().run()) % 1000003;
checksum = (checksum + new Runner61().run()) % 1000003;
checksum = (checksum + new Runner62().run()) % 1000003;
checksum = (checksum + new Runner63().run()) % 1000003;
checksum = (checksum + new Runner64().run()) % 1000003;
checksum = (checksum + new Runner65().run()) % 1000003;
checksum = (checksum + new Runner66().run()) % 1000003;
checksum = (checksum + new Runner67().run()) % 1000003;
checksum = (checksum + new Runner68().run()) % 1000003;
checksum = (checksum + new Runner69().run()) % 1000003;
checksum = (checksum + new Runner70().run()) % 1000003;
checksum = (checksum + new Runner71().run()) % 1000003;
checksum = (checksum + new Runner72().run()) % 1000003;
checksum = (checksum + new Runner73().run()) % 1000003;
checksum = (checksum + new Runner74().run()) % 1000003;
checksum = (checksum + new Runner75().run()) % 1000003;
checksum = (checksum + new Runner76().run()) % 1000003;
checksum = (checksum + new Runner77().run()) % 1000003;
checksum = (checksum + new Runner78().run()) % 1000003;
checksum = (checksum + new Runner79().run()) % 1000003;
checksum = (checksum + new Runner80().run()) % 1000003;
checksum = (checksum + new Runner81().run()) % 1000003;
checksum = (checksum + new Runner82().run()) % 1000003;
checksum = (checksum + new Runner83().run()) % 1000003;
checksum = (checksum + new Runner84().run()) % 1000003;
checksum = (checksum + new Runner85().run()) % 1000003;
checksum = (checksum + new Runner86().run()) % 1000003;
checksum = (checksum + new Runner87().run()) % 1000003;
checksum = (checksum + new Runner88().run()) % 1000003;
checksum = (checksum + new Runner89().run()) % 1000003;
checksum = (checksum + new Runner90().run()) % 1000003;
checksum = (checksum + new Runner91().run()) % 1000003;
checksum = (checksum + new Runner92().run()) % 1000003;
checksum = (checksum + new Runner93().run()) % 1000003;
checksum = (checksum + new Runner94().run()) % 1000003;
checksum = (checksum + new Runner95().run()) % 1000003;
checksum = (checksum + new Runner96().run()) % 1000003;
checksum = (checksum + new Runner97().run()) % 1000003;
checksum = (checksum + new Runner98().run()) % 1000003;
checksum = (checksum + new Runner99().run()) % 1000003;
checksum = (checksum + new Runner100().run()) % 1000003;
checksum = (checksum + new Runner101().run()) % 1000003;
checksum = (checksum + new Runner102().run()) % 1000003;
checksum = (checksum + new Runner103().run()) % 1000003;
checksum = (checksum + new Runner104().run()) % 1000003;
checksum = (checksum + new Runner105().run()) % 1000003;
checksum = (checksum + new Runner106().run()) % 1000003;
checksum = (checksum + new Runner107().run()) % 1000003;
checksum = (checksum + new Runner108().run()) % 1000003;
checksum = (checksum + new Runner109().run()) % 1000003;
checksum = (checksum + new Runner110().run()) % 1000003;
checksum = (checksum + new Runner111().run()) % 1000003;
checksum = (checksum + new Runner112().run()) % 1000003;
checksum = (checksum + new Runner113().run()) % 1000003;
checksum = (checksum + new Runner114().run()) % 1000003;
checksum = (checksum + new Runner115().run()) % 1000003;
checksum = (checksum + new Runner116().run()) % 1000003;
checksum = (checksum + new Runner117().run()) % 1000003;
checksum = (checksum + new Runner118().run()) % 1000003;
checksum = (checksum + new Runner119().run()) % 1000003;
checksum = (checksum + new Runner120().run()) % 1000003;
checksum = (checksum + new Runner121().run()) % 1000003;
checksum = (checksum + new Runner122().run()) % 1000003;
checksum = (checksum + new Runner123().run()) % 1000003;
checksum = (checksum + new Runner124().run()) % 1000003;
checksum = (checksum + new Runner125().run()) % 1000003;
checksum = (checksum + new Runner126().run()) % 1000003;
checksum = (checksum + new Runner127().run()) % 1000003;
checksum = (checksum + new Runner128().run()) % 1000003;
checksum = (checksum + new Runner129().run()) % 1000003;
checksum = (checksum + new Runner130().run()) % 1000003;
checksum = (checksum + new Runner131().run()) % 1000003;
checksum = (checksum + new Runner132().run()) % 1000003;
checksum = (checksum + new Runner133().run()) % 1000003;
checksum = (checksum + new Runner134().run()) % 1000003;
checksum = (checksum + new Runner135().run()) % 1000003;
checksum = (checksum + new Runner136().run()) % 1000003;
checksum = (checksum + new Runner137().run()) % 1000003;
checksum = (checksum + new Runner138().run()) % 1000003;
checksum = (checksum + new Runner139().run()) % 1000003;
checksum = (checksum + new Runner140().run()) % 1000003;
checksum = (checksum + new Runner141().run()) % 1000003;
checksum = (checksum + new Runner142().run()) % 1000003;
checksum = (checksum + new Runner143().run()) % 1000003;
checksum = (checksum + new Runner144().run()) % 1000003;
checksum = (checksum + new Runner145().run()) % 1000003;
checksum = (checksum + new Runner146().run()) % 1000003;
checksum = (checksum + new Runner147().run()) % 1000003;
checksum = (checksum + new Runner148().run()) % 1000003;
checksum = (checksum + new Runner149().run()) % 1000003;
checksum = (checksum + new Runner150().run()) % 1000003;
checksum = (checksum + new Runner151().run()) % 1000003;
checksum = (checksum + new Runner152().run()) % 1000003;
checksum = (checksum + new Runner153().run()) % 1000003;
checksum = (checksum + new Runner154().run()) % 1000003;
checksum = (checksum + new Runner155().run()) % 1000003;
checksum = (checksum + new Runner156().run()) % 1000003;
checksum = (checksum + new Runner157().run()) % 1000003;
checksum = (checksum + new Runner158().run()) % 1000003;
checksum = (checksum + new Runner159().run()) % 1000003;
checksum = (checksum + new Runner160().run()) % 1000003;
checksum = (checksum + new Runner161().run()) % 1000003;
checksum = (checksum + new Runner162().run()) % 1000003;
checksum = (checksum + new Runner163().run()) % 1000003;
checksum = (checksum + new Runner164().run()) % 1000003;
checksum = (checksum + new Runner165().run()) % 1000003;
checksum = (checksum + new Runner166().run()) % 1000003;
checksum = (checksum + new Runner167().run()) % 1000003;
checksum = (checksum + new Runner168().run()) % 1000003;
checksum = (checksum + new Runner169().run()) % 1000003;
checksum = (checksum + new Runner170().run()) % 1000003;
checksum = (checksum + new Runner171().run()) % 1000003;
checksum = (checksum + new Runner172().run()) % 1000003;
checksum = (checksum + new Runner173().run()) % 1000003;
checksum = (checksum + new Runner174().run()) % 1000003;
checksum = (checksum + new Runner175().run()) % 1000003;
checksum = (checksum + new Runner176().run()) % 1000003;
checksum = (checksum + new Runner177().run()) % 1000003;
checksum = (checksum + new Runner178().run()) % 1000003;
checksum = (checksum + new Runner179().run()) % 1000003;
checksum = (checksum + new Runner180().run()) % 1000003;
checksum = (checksum + new Runner181().run()) % 1000003;
checksum = (checksum + new Runner182().run()) % 1000003;
checksum = (checksum + new Runner183().run()) % 1000003;
checksum = (checksum + new Runner184().run()) % 1000003;
checksum = (checksum + new Runner185().run()) % 1000003;
checksum = (checksum + new Runner186().run()) % 1000003;
checksum = (checksum + new Runner187().run()) % 1000003;
checksum = (checksum + new Runner188().run()) % 1000003;
checksum = (checksum + new Runner189().run()) % 1000003;
checksum = (checksum + new Runner190().run()) % 1000003;
checksum = (checksum + new Runner191().run()) % 1000003;
checksum = (checksum + new Runner192().run()) % 1000003;
checksum = (checksum + new Runner193().run()) % 1000003;
checksum = (checksum + new Runner194().run()) % 1000003;
checksum = (checksum + new Runner195().run()) % 1000003;
checksum = (checksum + new Runner196().run()) % 1000003;
checksum = (checksum + new Runner197().run()) % 1000003;
checksum = (checksum + new Runner198().run()) % 1000003;
checksum = (checksum + new Runner199().run()) % 1000003;
printf("modules=%d checksum=%d\n", 200, checksum);
