## Building

```
    gcc -o server -Wall server.c
```

## Invoking

```
    ./server /path/to/big/file
```

## Testing

On the same host:

```
    nc localhost 4443 > /dev/null
```    
