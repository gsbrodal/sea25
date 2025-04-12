import matplotlib.pyplot as plt

data_folder = '../data/'
plot_folder = '../plots/'

algorithms = [
    'successor, no compression',
    'successor, recursive',
    'successor, 2-pass',
    'successor, 2-pass, checked',
    'successor, halving',
    'union find',
    'quick find',
    'successor, 2-pass, microset',
    'union find, microset',
    'quick find, microset',
]

color = {alg: f'C{i}' for i, alg in enumerate(algorithms)}
for alg in algorithms:
    if alg.endswith(', microset'):
        color[alg] = color[alg.removesuffix(', microset')]
linestyle = {alg: 'dashed' if 'microset' in alg else 'solid' for alg in algorithms}

def load_data(filename):
    '''Load rows of data from a CSV file, skip comment lines starting with #'''

    with open(data_folder + filename, 'r') as f:
        return list(eval(line) for line in f)

def figure(data, filename=None, show=False, legend=True, logx=False, logy=False, xlim=None, ylim=None, title=None):
    '''Plot data from a list of rows, each row is (algorithm, n, total time, query time)'''
    
    algs = sorted({row[0] for row in data}, key=lambda x: algorithms.index(x))
    
    plt.figure()
    for algorithm in algs:
        rows = [row for row in data if row[0] == algorithm]
        x = [row[2] for row in rows]
        y = [row[3] / row[2] for row in rows]
        plt.plot(x, y, '.', color=color[algorithm], linestyle=linestyle[algorithm], label=algorithm)

    if logx: 
        plt.xscale('log')
    if logy: 
        plt.yscale('log')
    plt.xlim(xlim)
    plt.ylim(ylim)
    plt.title(title)
    plt.xlabel('$n$')
    plt.gca().xaxis.set_label_coords(0.99, -0.025)
    plt.ylabel('Total time (seconds) / $n$')
    if legend:
        plt.legend(frameon=False, loc='upper left')
    if filename:
        file = plot_folder + filename
        print('Saving', file)
        plt.savefig(file, bbox_inches='tight')
    if show or not filename:
        plt.show()

data = load_data('data.csv')      # row = alg, input, n, time
data.sort(key=lambda row:row[2])  # sort by n

title = r'Delete$(1,\ldots, n)$ + $n$ Succ(1)'
rows = [row for row in data if row[1] == 'query_one'] # and row[2] <= 65536]
figure(rows, 'naive-logarithmic.pdf', logx=True, logy=True, xlim=(1, None), show=True, title=title)
rows = [row for row in data if row[1] == 'query_one' and row[0] != 'successor, no compression']
figure(rows, 'successor-query-one.pdf', logx=True, ylim=(0,None), legend=False, show=True, title=title)

legend = True
for p in range(-3, 4):
    q = f'{1.0 * 2 ** p:.3f}'
    if p == 0:
        label = 'n'
    elif p < 0:
        label = r'\frac{1}{' + str(2**-p) + '}n'
    elif p > 0:
        label = f'{2**p}n'
    
    rows = [row for row in data if row[1] == f'worst_case {q}']
    figure(rows, f'successor-query-worst-case-{q}.pdf', legend=legend, logx=True, ylim=(0,None), show=True,
           title=r'Delete$(1,\ldots,n)$, $' + label + r'$ Succ(worst)')
    
    rows = [row for row in data if row[1] == f'random {q}']
    figure(rows, f'random-deletion-worst-case-query-{q}.pdf', legend=legend, logx=True, ylim=(0,None), show=True,
           title=r'$n$ Delete(random), $' + label + r'$ Succ(worst)')
    legend = False

rows = [row for row in data if row[1] == 'worst_case 1.000']
figure(rows, f'successor-query-worst-case.pdf', legend=True, logx=True, ylim=(0,None), show=True,
       title=r'Delete$(1,\ldots,n)$, $n$ Succ(worst)')

rows = [row for row in data if row[1] == 'random 1.000']
figure(rows, f'random-deletion-worst-case-query.pdf', legend=False, logx=True, ylim=(0,None), show=True,
       title=r'$n$ Delete(random), $n$ Succ(worst)')
