"""Configuration settings for the API service."""

import os
import sys
from pathlib import Path

# Optional: load .env file if python-dotenv is installed
try:
    from dotenv import load_dotenv
    load_dotenv()
except ImportError:
    pass  # dotenv not installed, use env vars directly


class Settings:
    """Application settings with auto-detection for Windows/Linux."""
    
    def __init__(self):
        # Determine project root (api/ is one level down from root)
        self._api_dir = Path(__file__).parent.parent.resolve()
        self._project_root = self._api_dir.parent.resolve()
    
    @property
    def ENGINE_PATH(self) -> str:
        """
        Get the engine binary path.
        
        Search order:
        1. ENGINE_PATH environment variable (if set)
        2. build/engine/Debug/exchange_engine.exe (Windows Debug)
        3. build/engine/Release/exchange_engine.exe (Windows Release)
        4. build/engine/exchange_engine.exe (Windows flat)
        5. build/engine/Debug/exchange_engine (Linux Debug)
        6. build/engine/Release/exchange_engine (Linux Release)
        7. build/engine/exchange_engine (Linux flat)
        """
        # Check environment variable first
        env_path = os.getenv("ENGINE_PATH")
        if env_path:
            p = Path(env_path)
            if p.exists():
                return str(p.resolve())
            # If env var set but doesn't exist, warn and continue searching
            print(f"Warning: ENGINE_PATH={env_path} does not exist, searching...", file=sys.stderr)
        
        # Determine if we're on Windows
        is_windows = sys.platform.startswith('win')
        exe_suffix = ".exe" if is_windows else ""
        
        # Search paths in priority order
        search_paths = [
            self._project_root / "build" / "engine" / "Debug" / f"exchange_engine{exe_suffix}",
            self._project_root / "build" / "engine" / "Release" / f"exchange_engine{exe_suffix}",
            self._project_root / "build" / "engine" / f"exchange_engine{exe_suffix}",
        ]
        
        for path in search_paths:
            if path.exists():
                return str(path.resolve())
        
        # Return the most likely path (for error messages)
        default = search_paths[0]
        return str(default)

    @property
    def DATA_DIR(self) -> str:
        """Absolute path to data directory."""
        env_val = os.getenv("DATA_DIR")
        if env_val:
            return str(Path(env_val).resolve())
        # Default: project_root/data (not relative to cwd)
        return str((self._project_root / "data").resolve())
    
    @property
    def EVENT_LOG_PATH(self) -> str:
        """Absolute path to event log file."""
        env_val = os.getenv("EVENT_LOG_PATH")
        if env_val:
            return str(Path(env_val).resolve())
        return str(Path(self.DATA_DIR) / "events.jsonl")
    
    @property
    def SNAPSHOT_DIR(self) -> str:
        """Absolute path to snapshot directory."""
        env_val = os.getenv("SNAPSHOT_DIR")
        if env_val:
            return str(Path(env_val).resolve())
        return str(Path(self.DATA_DIR) / "snapshots")

    # Rate limiting settings
    @property
    def RATE_LIMIT_REQUESTS(self) -> int:
        return int(os.getenv("RATE_LIMIT_REQUESTS", "100"))
    
    @property
    def RATE_LIMIT_WINDOW_SECONDS(self) -> int:
        return int(os.getenv("RATE_LIMIT_WINDOW_SECONDS", "60"))

    # API authentication
    API_KEY_HEADER: str = "X-API-Key"
    VALID_API_KEYS: set[str] = {
        "test-key-1", 
        "test-key-2", 
        "dev-key",
        "aztec-demo-key",
    }
    
    # Server settings
    @property
    def HOST(self) -> str:
        return os.getenv("HOST", "127.0.0.1")
    
    @property
    def PORT(self) -> int:
        return int(os.getenv("PORT", "8000"))
    
    def print_config(self):
        """Print current configuration for debugging."""
        print(f"Project Root:    {self._project_root}")
        print(f"ENGINE_PATH:     {self.ENGINE_PATH}")
        print(f"  exists:        {Path(self.ENGINE_PATH).exists()}")
        print(f"DATA_DIR:        {self.DATA_DIR}")
        print(f"EVENT_LOG_PATH:  {self.EVENT_LOG_PATH}")
        print(f"SNAPSHOT_DIR:    {self.SNAPSHOT_DIR}")


# Global settings instance
settings = Settings()


# Quick self-test when run directly
if __name__ == "__main__":
    settings.print_config()