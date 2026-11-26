"""Optional fragmentation-over-time plot (needs matplotlib)."""


def plot_timelines(results, title, path):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not installed: skipping plot (pip install matplotlib)")
        return False
    plt.figure(figsize=(9, 4.5))
    for r in results:
        if r["key"] in ("first", "best", "memsense", "paged"):
            plt.plot(r["frag_timeline"], label=r["name"], linewidth=2.5 if r["key"] == "memsense" else 1.2)
    plt.title(f"Fragmentation over time: {title}")
    plt.xlabel("time (samples)")
    plt.ylabel("fragmentation score (lower is better)")
    plt.legend()
    plt.grid(alpha=0.3)
    plt.tight_layout()
    plt.savefig(path, dpi=120)
    plt.close()
    return True
