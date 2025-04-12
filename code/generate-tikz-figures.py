# Script to generate figures for the paper

import sys
import random    
from math import log2, ceil
import matplotlib.pyplot as plt

sys.setrecursionlimit(1_000_000)


class SuccDelete:
    def __init__(self, n):
        A = self.A = list(range(n + 1))
        self.n = n
        A[0] = None  # assume 1-indexed

    def delete(self, i):
        A = self.A
        A[i] = i + 1
    
    def successor(self, i):
        A = self.A
        if A[i] > i: 
            A[i] = self.successor(A[i])
        return A[i]


class SuccPredDelete:
    def __init__(self, n):
        self.n = n
        A = self.A = [None] * (n + 1)
        A[1] = 1
        for i in range(2, n + 1):
            A[i] = i - 1

    def delete(self, i):
        A = self.A
        if A[i] <= i:
            A[self.successor(i + 1)] = A[i]
            A[i] = i + 1
    
    def successor(self, i):
        A = self.A
        if A[i] <= i: 
            return i
        A[i] = self.successor(A[i])
        return A[i]

    def predecessor(self, i):
        A = self.A
        if A[i] <= i:
            return i
        return A[self.successor(i)]


def find_interesting_state(n=20, decreases=5, minimum_set_size=5):
    while True:
        print('.', end='', flush=True)
        I = SuccDelete(n)
        L = list(range(1, n+1))

        while len(L) > 1:
            i = random.choice(L[:-1])
            I.delete(i)
            L.remove(i)
            for _ in range(5):
                j = random.randint(1, n - 1)
                k = I.successor(j)
            niceness = sum(I.A[i] > I.A[i+1] for i in range(1, n))
            if niceness >= decreases and len(L) >= minimum_set_size:
                return I

def find_interesting_succ_pred_state(n=20, decreases=5, minimum_set_size=5):
    while True:
        print('.', end='', flush=True)
        I = SuccPredDelete(n)
        L = list(range(2, n))

        while L:
            i = random.choice(L)
            I.delete(i)
            L.remove(i)
            for _ in range(5):
                j = random.randint(2, n - 1)
                k = I.successor(j)
            for _ in range(5):
                j = random.randint(2, n - 1)
                k = I.predecessor(j)
            niceness = sum(I.A[i] > I.A[i+1] > i for i in range(1, n))
            if niceness >= decreases and len(L) >= minimum_set_size and all(abs(i - I.A[i]) <= 6 for i in range(1, n+1)):
                return I


def tikz(A):
  n = len(A) - 1

  S = ', '.join(f'{i}/{A[i]}' for i in range(1, n + 1) if A[i] <= i)
  forwards = ', '. join(f'{i}/{A[i]}' for i in range(1, n + 1) if A[i] > i)
  backwards = ', '. join(f'{i}/{A[i]}' for i in range(1, n + 1) if A[i] < i)
  
  return r'''\begin{tikzpicture}[scale=0.5]
  \draw (0.5,0) rectangle (''' + str(n) + r'''.5,1);
  \foreach \i in {1, ..., ''' + str(n-1) + r'''} \draw (\i+0.5, 0) -- ++(0, 1);
  \foreach \i in {1, ..., ''' + str(n) + r'''} \node at (\i, 1.5) {\scriptsize \i};
  \foreach \i/\j in {''' + forwards + r'''} {
    \node at (\i, 0.5) {\j};
    \path[-latex] (\i, -0.1) edge[bend right=30] (\j, -0.1);
  }''' + ('' if not backwards else r'''
  \foreach \i/\j in {''' + backwards + r'''} {
    \path[-latex] (\i, 1.9) edge[bend right=30] (\j, 1.9);
  }''') + r'''
  \foreach \i/\j in {''' + S + r'''} \node at (\i, 0.5) {\textbf{\j}};
  \node at (-0.25, 0.5) {$A$};
\end{tikzpicture}'''


print(tikz(find_interesting_state(n=25).A))
print(tikz(find_interesting_succ_pred_state(n=25).A))
