import logging
import sys

from .config import parse_args
from .server import create_server


def main():
    config = parse_args()
    logging.basicConfig(
        level=logging.WARNING, stream=sys.stderr, format="%(levelname)s %(name)s: %(message)s"
    )
    create_server(config).run(transport="stdio")


if __name__ == "__main__":
    main()
