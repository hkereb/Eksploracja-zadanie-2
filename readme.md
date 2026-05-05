# Instrukcja

```
    python -m venv venv
    pip install -r "requirements.txt"
    cd data
    python data/download.py
```

# Zadanie

W tym zadaniu wcielacie się w programistów, których zadaniem jest napisanie efektywnej metody odkrywania reguł asocjacyjnych. Pamiętajcie - liczy się ostateczny czas, więc aspekty takie jak dobór języka programowania, dobór algorytmów, sposób kompilacji i uruchomienia, zrównoleglenie, dobór bibliotek... mogą mieć wpływ na rozwiązanie.

Czas przetwarzania zbioru będzie miał bezpośrednie przełożenie na liczbę uzystkanych punków.

Algorytmy będą testowane z użyciem zbioru, który można pobrać za pomocą skryptu w katalogu data.

# Materiały 

Do dyspozycji masz rozwiązania problemu napisane w języku python:
 - slow_solution_apriori.py : implementuje "od zera" algorytm apriori
 - slow_solution_fpgrowth.py implementuje "od zera" algorytm FP-Growth

# Interfejs

Twoje rozwiązanie powinno implementować metodę
```solve(min_support, min_confidence, verbose=False)```

Bez względu na język programowania, metoda ta jest owinięciem twojego rozwiązania w języku python.

Parametr verbose kontroluje to, czy wywołanie generuje informacje o wykonaniu.

Metoda powinna zwrócić listę reguł postaci:

A=>B, support = s, confidence = c

zakodowanych jako słownik:
```
{
    'A' : List,
    'B' : List,
    'supp' : number,
    'conf' : number
}
```

