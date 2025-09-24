#!/usr/bin/env python3
import os
import argparse
import requests
import time

def format_sequence(seq):
    seq_str = f"{seq:09d}"
    return seq_str[:3], seq_str[3:6], seq_str[6:9]

def download_file(url, local_path):
    headers = {"User-Agent": "OsmChangeDownloader"}
    try:
        response = requests.get(url, stream=True, headers=headers)
        if response.status_code != 200:
            return False
        os.makedirs(os.path.dirname(local_path), exist_ok=True)
        with open(local_path, "wb") as f:
            for chunk in response.iter_content(chunk_size=8192):
                if chunk:
                    f.write(chunk)
        return True
    except Exception as e:
        print(f"Exception while downloading {url}: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description="Download files using sequence number-based URL paths."
    )
    parser.add_argument("base_url", help="Base URL for downloads")
    parser.add_argument("sequence_number", type=int,
                        help="Starting sequence number")
    args = parser.parse_args()

    start_time = time.time()

    seq = args.sequence_number
    while True:
        a, b, c = format_sequence(seq)
        local_folder = os.path.join(a, b)
        osc_filename = f"{c}.osc.gz"
        state_filename = f"{c}.state.txt"
        local_osc_path = os.path.join(local_folder, osc_filename)
        local_state_path = os.path.join(local_folder, state_filename)

        base = args.base_url.rstrip('/')
        osc_url = f"{base}/{a}/{b}/{osc_filename}"
        state_url = f"{base}/{a}/{b}/{state_filename}"

        print(f"Downloading {osc_url}...")
        if not download_file(osc_url, local_osc_path):
            elapsed_time = time.time() - start_time
            print(f"Error encountered. Stopping. Total time: {elapsed_time:.2f} seconds.")
            break

        print(f"Downloading {state_url}...")
        if not download_file(state_url, local_state_path):
            elapsed_time = time.time() - start_time
            print(f"Error encountered. Stopping. Total time: {elapsed_time:.2f} seconds.")
            break

        seq += 1

if __name__ == "__main__":
    main()
