import kagglehub
import os

download_path = kagglehub.dataset_download("mashlyn/online-retail-ii-uci")

csv_filename = "online_retail_II.csv"
full_path = os.path.join(download_path, csv_filename)

with open('config.py', 'a', encoding='utf-8') as f:
    f.write(f'datapath = {repr(full_path)}')

print(f"Sukces! Ścieżka zapisana w config.py: {full_path}")