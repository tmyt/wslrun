# wslrun

Small launcher for wsl. Run argv[0] in default WSL distribution.

## usage

Copy or symlink to WSL command name.

```
// copy
CMD> copy wslrun.exe zsh.exe
// symlink
CMD> mklink zsh.exe wslrun.exe
```

Run copy or symlink file.

```
CMD> zsh.exe
// launch zsh in default WSL distribution
wsl$
```

## config

You can override distribution. create `wslrun.config` in same executable file directory.

```
[config]
# Your favorite distribution name
name=Debian
```

## license

MIT