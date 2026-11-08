# Reduces the table rows that bench prints, several runs per allocator, to
# one row each: the median time in microseconds and the median anonymous RSS
# in MiB. The runs files keep every run, for the spread. Rows keep the order
# they first appear in. Lines that are not table rows, such as the prompt
# line, are skipped.
#
# Plain POSIX awk, because the image ships mawk.

BEGIN { FS = "|" }

# Rounds to three significant figures, or to a whole number above 1000.
function sig3(v,    d, x) {
    d = 2
    for (x = v; x >= 10 && d > 0; x /= 10) d--
    for (x = v; x > 0 && x < 1; x *= 10) d++
    return sprintf("%." d "f", v)
}

# The median of the n values in a[1..n], which it sorts in place.
function median(a, n,    i, j, t) {
    for (i = 2; i <= n; i++) {
        t = a[i]
        for (j = i - 1; j >= 1 && a[j] > t; j--) a[j + 1] = a[j]
        a[j + 1] = t
    }
    return n % 2 ? a[(n + 1) / 2] : (a[n / 2] + a[n / 2 + 1]) / 2
}

/^\| / {
    name = $2
    gsub(/^ +| +$/, "", name)
    if (!(name in runs)) order[++names] = name
    k = ++runs[name]
    # awk reads the number at the front of "1714.0 ns" and drops the unit.
    ns[name, k] = $3 + 0
    live[name, k] = $4 + 0
    freed[name, k] = $5 + 0
}

END {
    printf "| Allocator | Time per free and malloc (µs), median of %d runs " \
        "| Anonymous RSS, blocks live (MiB) | Anonymous RSS, all freed (MiB) |\n", runs[order[1]]
    print "|---|--:|--:|--:|"
    for (i = 1; i <= names; i++) {
        name = order[i]
        n = runs[name]
        for (k = 1; k <= n; k++) {
            t[k] = ns[name, k] / 1000
            l[k] = live[name, k] / 1024
            f[k] = freed[name, k] / 1024
        }
        printf "| %s | %s | %s | %s |\n", name, sig3(median(t, n)), sig3(median(l, n)),
            sig3(median(f, n))
    }
}
