echo "Working dir: $(pwd)"
echo "Lock limit: $(ulimit -l)"
lscpu | grep -E 'CPU\(s\)|Thread|Core'
cat /proc/meminfo | grep -E 'MemTotal|MemFree|SwapTotal|SwapFree'

 head -c 1500M < /dev/urandom | pv -s 1500M > $(pwd)/input
$(pwd)/external_sort
rm -f $(pwd)/input
rm -f $(pwd)/output