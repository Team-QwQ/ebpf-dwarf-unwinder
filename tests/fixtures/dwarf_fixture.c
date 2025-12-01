int target_function(int x)
{
    return x + 1;
}

int main(void)
{
    return target_function(41) == 42 ? 0 : 1;
}
