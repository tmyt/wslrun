# wslrun

Small launcher for wsl. Run argv[0] in default WSL distribution.

## usage

Copy or hardlink[^1] to WSL command name.

```
// copy
CMD> copy wslrun.exe zsh.exe
// hardlink
CMD> mklink /H zsh.exe wslrun.exe
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

[^1]: Windows Explorer follows symlink and execute original executable path. It is not suitable for this program.