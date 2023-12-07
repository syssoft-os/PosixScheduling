import pandas as pd
import matplotlib.pyplot as plt
import sys

def plot_row(row, row_number):
    # Extract the title from the first element of the row
    title = row.iloc[0]
    print(f'Plotting row {row_number} with title {title}')
    # Remove the first element from the row
    row = row.iloc[1:]
    plt.figure(figsize=(10, 6))
    plt.plot(row)
    plt.title(f'Time Series Plot for Row {title}')
    plt.xlabel('Burst Number')
    plt.ylabel('ms')
    plt.show()

def main(filename):
    # Read the CSV file
    df = pd.read_csv(filename,header=None)
    # Iterate over each row and plot it
    for i in range(len(df)):
        plot_row(df.iloc[i], i)

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python plot_csv.py <filename>")
    else:
        main(sys.argv[1])