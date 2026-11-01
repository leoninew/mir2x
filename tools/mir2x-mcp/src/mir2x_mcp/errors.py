from dataclasses import dataclass


@dataclass
class BridgeError(Exception):
    code: str
    message: str
    side_effects_possible: bool = False

    def payload(self) -> dict:
        return {
            "ok": False,
            "error": {
                "code": self.code,
                "message": self.message,
                "details": {
                    "retryable": False,
                    "side_effects_possible": self.side_effects_possible,
                },
            },
            "context": None,
        }

    def __str__(self):
        return self.code
