enum Operation {
    Add,
    Multiply
}

class Calculator {
    int value = 0;

    Calculator(int initial) {
        value = initial;
    }

    int add(int amount) {
        return value + amount;
    }

    int sum_to(int end) {
        int result = value;
        for (int i = 0; i < end; i += 1) {
            result += i;
        }
        return result;
    }
}

auto calculator = new Calculator(10);
printf("calculator result: %d\n", calculator.add(5));
