import pymupdf  # Changed from fitz
import os
import time
import argparse
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading
from pathlib import Path

try:
    from tqdm import tqdm
    HAS_TQDM = True
except ImportError:
    HAS_TQDM = False
    print("Note: Install 'tqdm' for a better progress bar: pip install tqdm")

class PDFCombinerFast:
    def __init__(self):
        self.lock = threading.Lock()
        self.combined = None
        self.processed_count = 0
        self.total_files = 0
        self.errors = []
        
    def combine_pdfs_parallel(self, input_folder=None, output_file="combined.pdf", 
                               max_workers=4, pdf_files=None, sort_files=True):
        """
        Multi-threaded PDF combining with folder or file list support.
        
        Args:
            input_folder (str): Folder containing PDF files
            output_file (str): Output PDF filename
            max_workers (int): Number of parallel threads
            pdf_files (list): List of specific PDF file paths
            sort_files (bool): Sort files alphabetically
        """
        start_time = time.time()
        self.errors = []
        
        # Get list of PDF files
        if pdf_files:
            files_to_process = [f for f in pdf_files if os.path.exists(f) and f.lower().endswith('.pdf')]
            if sort_files:
                files_to_process.sort()
        elif input_folder:
            if not os.path.exists(input_folder):
                print(f"Error: Folder '{input_folder}' does not exist.")
                return
            
            files_to_process = [os.path.join(input_folder, f) for f in os.listdir(input_folder) 
                               if f.lower().endswith('.pdf')]
            if sort_files:
                files_to_process.sort()
        else:
            print("Error: Please provide either input_folder or pdf_files.")
            return
        
        if not files_to_process:
            print("No PDF files found!")
            return
        
        self.total_files = len(files_to_process)
        print(f"Found {self.total_files} PDF files")
        print(f"Using {max_workers} worker threads")
        
        # Create a new PDF document
        self.combined = pymupdf.open()  # Changed from fitz.open()
        
        # Process files in parallel with progress bar
        if HAS_TQDM:
            pbar = tqdm(total=self.total_files, desc="Processing PDFs", unit="file")
        
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            futures = {}
            for file_path in files_to_process:
                future = executor.submit(self._process_file, file_path)
                futures[future] = file_path
            
            for future in as_completed(futures):
                self.processed_count += 1
                if HAS_TQDM:
                    pbar.update(1)
                    pbar.set_postfix({"Errors": len(self.errors)})
                else:
                    if self.processed_count % 10 == 0:
                        print(f"Processed {self.processed_count}/{self.total_files} files...")
        
        if HAS_TQDM:
            pbar.close()
        
        # Report errors if any
        if self.errors:
            print(f"\n⚠️  {len(self.errors)} files had errors:")
            for error in self.errors[:5]:  # Show first 5 errors
                print(f"  - {error}")
            if len(self.errors) > 5:
                print(f"  ... and {len(self.errors) - 5} more")
        
        # Save the combined PDF
        print("\nSaving combined PDF...")
        try:
            # Use compression for smaller file size
            self.combined.save(output_file, deflate=True)
            self.combined.close()
        except Exception as e:
            print(f"Error saving PDF: {e}")
            return
        
        elapsed = time.time() - start_time
        if os.path.exists(output_file):
            file_size = os.path.getsize(output_file) / (1024 * 1024)  # Size in MB
            print(f"\n✅ Successfully combined {self.total_files - len(self.errors)} PDFs")
            print(f"   Output: {output_file} ({file_size:.2f} MB)")
            print(f"   Time: {elapsed:.2f} seconds")
            if elapsed > 0:
                print(f"   Speed: {self.total_files/elapsed:.1f} files/second")
        else:
            print(f"\n❌ Failed to create output file: {output_file}")
    
    def _process_file(self, file_path):
        """Process a single PDF file."""
        filename = os.path.basename(file_path)
        try:
            doc = pymupdf.open(file_path)  # Changed from fitz.open()
            with self.lock:
                self.combined.insert_pdf(doc) # pyright: ignore[reportOptionalMemberAccess]
            doc.close()
            return True
        except Exception as e:
            error_msg = f"{filename}: {str(e)}"
            with self.lock:
                self.errors.append(error_msg)
            return False

def main():
    """Main function with argument parsing."""
    parser = argparse.ArgumentParser(
        description='Fast multi-threaded PDF combiner for large collections',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Combine all PDFs in a folder
  python pdf.py -f ./pdfs -o dictionary.pdf
  
  # Combine specific files
  python pdf.py -p file1.pdf file2.pdf file3.pdf -o combined.pdf
  
  # Use 8 threads for faster processing
  python pdf.py -f ./pdfs -w 8 -o fast_combined.pdf
  
  # Disable file sorting
  python pdf.py -f ./pdfs --no-sort
        """
    )
    
    parser.add_argument('-f', '--folder', 
                       help='Folder containing PDF files to combine')
    parser.add_argument('-p', '--files', nargs='+', 
                       help='Specific PDF files to combine')
    parser.add_argument('-o', '--output', default='combined.pdf',
                       help='Output filename (default: combined.pdf)')
    parser.add_argument('-w', '--workers', type=int, default=4,
                       help='Number of worker threads (default: 4)')
    parser.add_argument('--no-sort', action='store_true',
                       help='Disable alphabetical sorting of files')
    parser.add_argument('-q', '--quiet', action='store_true',
                       help='Suppress progress output')
    
    # For backward compatibility: support positional arguments
    parser.add_argument('positional_files', nargs='*', 
                       help=argparse.SUPPRESS)
    
    args = parser.parse_args()
    
    # Handle positional arguments for backward compatibility
    if args.positional_files and not args.files and not args.folder:
        # Check if positional arg is a folder or files
        if len(args.positional_files) == 1 and os.path.isdir(args.positional_files[0]):
            args.folder = args.positional_files[0]
        else:
            args.files = args.positional_files
    
    # Validate input
    if not args.folder and not args.files:
        parser.print_help()
        print("\nError: Please specify either --folder or --files")
        sys.exit(1)
    
    # Check for tqdm and handle quiet mode
    if args.quiet:
        global HAS_TQDM
        HAS_TQDM = False
    
    # Combine PDFs
    combiner = PDFCombinerFast()
    combiner.combine_pdfs_parallel(
        input_folder=args.folder,
        output_file=args.output,
        max_workers=args.workers,
        pdf_files=args.files,
        sort_files=not args.no_sort
    )

if __name__ == "__main__":
    main()