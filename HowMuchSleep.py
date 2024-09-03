import pandas as pd
import argparse
import re
from datetime import datetime, timedelta
import matplotlib.pyplot as plt

csv_file_path = 'ESP32_data.csv'

parser = argparse.ArgumentParser(description='Process pressure data for sleep evaluation.')
parser.add_argument('days', type=int, help='Number of days to consider for the evaluation of the hours of sleep')
args = parser.parse_args() # Argument parsing

df = pd.read_csv(csv_file_path, header=None, names=['Timestamp', 'Pressure'])
df['Timestamp'] = pd.to_datetime(df['Timestamp'], format='%Y-%m-%d %H:%M:%S')

def is_valid_pressure(value):
    if pd.isna(value):
        return False
    return re.match(r'^\d+$', str(value)) is not None

df['Pressure'] = df['Pressure'].astype(str)
df = df[df['Pressure'].apply(is_valid_pressure)] # Filter out invalid pressure values
df['Pressure'] = df['Pressure'].astype(int)

# Date range to filter the DataFrame, starting from yesterday
end_date = datetime.now().replace(hour=0, minute=0, second=0, microsecond=0)
start_date = end_date - timedelta(days=args.days)
df_filtered = df[(df['Timestamp'] >= start_date) & (df['Timestamp'] < end_date)]

df_filtered = df_filtered.sort_values(by='Timestamp').reset_index(drop=True) # Dataset sorting

df_filtered['Duration'] = df_filtered['Timestamp'].diff().fillna(pd.Timedelta(seconds=0)).dt.total_seconds() # Duration between consecutive timestamps

df_filtered['sleep'] = df_filtered['Pressure'].apply(lambda x: 1 if x > 2250 else 0) # If pressure > 2250 -> I was in bed

df_filtered['SleepSeconds'] = df_filtered.apply(lambda row: row['Duration'] if row['sleep'] == 1 else 0, axis=1)

# Group by date and sum the sleep seconds
df_filtered['Date'] = df_filtered['Timestamp'].dt.date
daily_sleep = df_filtered.groupby('Date')['SleepSeconds'].sum().reset_index()
daily_sleep['SleepHours'] = daily_sleep['SleepSeconds'] / 3600

total_sleep_seconds = df_filtered['SleepSeconds'].sum()
total_sleep_hours = total_sleep_seconds / 3600
total_period_hours = args.days * 24

sleep_ratio = total_sleep_hours / total_period_hours

print("Sleep hours each day:")
for index, row in daily_sleep.iterrows():
    print(f"Date: {row['Date']}, Sleep Hours: {row['SleepHours']:.2f} hours")

print(f"\nTotal sleep hours in the last {args.days} days starting from yesterday: {total_sleep_hours:.2f} hours")
print(f"Ratio of sleep hours to total hours in the period considered: {sleep_ratio:.2f}")

# Data filtering for better plotting it
df_resampled = df_filtered[['Timestamp', 'Pressure']].set_index('Timestamp').resample('1T').mean()
df_resampled['Pressure'] = df_resampled['Pressure'].ffill()
df_resampled['Pressure_MA'] = df_resampled['Pressure'].rolling(window=60, min_periods=1).mean()
start_date = end_date - timedelta(days=7)
df_last_week = df_resampled[(df_resampled.index >= start_date) & (df_resampled.index < end_date)]

# Plotting
plt.figure(figsize=(12, 6))
plt.plot(df_last_week.index, df_last_week['Pressure_MA'], label='Moving Average (1H)')
plt.scatter(df_last_week.index, df_last_week['Pressure'], color='red', label='Original Data')
plt.title('Pressure Data with Moving Average (1 Hour)')
plt.xlabel('Timestamp')
plt.ylabel('Pressure')
plt.legend()
plt.grid(True)
plt.xticks(rotation=45)
plt.tight_layout()

image_path = 'pressure_data_plot.png'
plt.savefig(image_path)
plt.show()

print(f"Plot saved as {image_path}")
