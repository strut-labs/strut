struct Number { int value; }
struct Config { int value; }

operator +(Number a, Number b) -> Number {
    return Number { value: a.value + b.value };
}

operator ==(Number a, Number b) -> bool {
    return a.value == b.value;
}

operator &(Number a, Number b) -> Number {
    return Number { value: a.value & b.value };
}

operator <<(Number value, int amount) -> Number {
    return Number { value: value.value << amount };
}

operator -(Number value) -> Number {
    return Number { value: -value.value };
}

operator :=(Number destination, Config source) -> void {
    destination.value = source.value;
}

operator =(Number& destination, Config source) -> void {
    destination.value = source.value;
}

function main() -> int {
    one := Number { value: 1 };
    two := Number { value: 2 };
    sum := one + two;
    same := sum == Number { value: 3 };
    masked := sum & Number { value: 1 };
    shifted := masked << 2;
    negative := -shifted;
    Number configured := Config { value: 7 };
    configured = Config { value: 9 };
    println(same);
    println(negative.value);
    println(configured.value);
    return 0;
}
