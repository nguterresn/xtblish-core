import http.server
import socketserver
import os
import argparse
import json


class BinaryFileHandler(http.server.SimpleHTTPRequestHandler):
    """Custom request handler that serves a specific binary file."""

    def __init__(self, *args, binary_file=None, **kwargs):
        self.binary_file = binary_file
        super().__init__(*args, **kwargs)

    def do_GET(self):
        if self.path == "/status":
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()

            file_exists = os.path.exists(self.binary_file)
            status_info = {
                "file_exists": file_exists,
                "file_path": self.binary_file,
                "file_name": (
                    os.path.basename(self.binary_file) if file_exists else None
                ),
                "file_size": os.path.getsize(self.binary_file) if file_exists else 0,
            }

            self.wfile.write(json.dumps(status_info).encode())
            print(
                f"Status request: File {'exists' if file_exists else 'does not exist'}"
            )
            return
        # If path is root or the specific file endpoint, serve our binary file
        if self.path == "/download":
            if self.binary_file and os.path.exists(self.binary_file):
                # Get file size for Content-Length header
                file_size = os.path.getsize(self.binary_file)
                file_name = os.path.basename(self.binary_file)

                # Set headers for binary file download
                self.send_response(200)
                self.send_header("Content-Type", "application/octet-stream")
                self.send_header("Content-Length", str(file_size))
                self.send_header(
                    "Content-Disposition", f'attachment; filename="{file_name}"'
                )
                self.end_headers()

                # Serve the file in binary mode
                with open(self.binary_file, "rb") as file:
                    self.wfile.write(file.read())

                print(f"Served file: {self.binary_file} ({file_size} bytes)")
                return
            else:
                self.send_error(404, "File not found")
                return
        else:
            # For any other path, use default behavior
            super().do_GET()


def run_server(port, file_path: str):
    """
    Run the HTTP server to serve the specified binary file.

    Args:
        port (int): Port to run the server on
        file_path (str): Path to the binary file to serve
    """
    # Ensure the file exists
    # if not os.path.exists(file_path):
    #     print(f"Error: File not found: {file_path}")
    #     return

    # Create handler with the binary file
    handler = lambda *args, **kwargs: BinaryFileHandler(
        *args, binary_file=file_path, **kwargs
    )

    # Start the server
    with socketserver.TCPServer(("", port), handler) as httpd:
        file_name = os.path.basename(file_path) if len(file_path) > 0 else ""
        file_size = os.path.getsize(file_path) if len(file_path) > 0 else ""

        print(f"Server started at http://localhost:{port}")
        print(f"Check the server status at http://localhost:{port}/status")
        if len(file_path):
            print(f"Serving file: {file_name} ({file_size} bytes)")
            print(f"File available at: http://localhost:{port}/download")

        print(f"\n\nPress Ctrl+C to stop the server")

        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer stopped.")


def main():
    parser = argparse.ArgumentParser(description="Serve a binary file via HTTP")
    parser.add_argument(
        "-f", "--file", default="", type=str, help="Path to the binary file to serve"
    )
    parser.add_argument(
        "-p",
        "--port",
        type=int,
        default=8080,
        help="Port to run the server on (default: 8080)",
    )
    args = parser.parse_args()

    run_server(args.port, args.file)


if __name__ == "__main__":
    main()
