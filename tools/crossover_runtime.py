"""Mac-hosted Windows PE verification; not native Windows qualification."""
import os
from pathlib import Path

WINE = Path('/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/bin/wine')


def windows_path(path):
    # Dedicated CrossOver verification bottles retain the standard Z: -> / map.
    return 'Z:' + str(Path(path).resolve()).replace('/', '\\')


def runtime_command(executable, arguments, directory, bottle=None):
    if not bottle:
        return [str(executable), *arguments]
    wine = Path(os.environ.get('CROSSOVER_WINE', str(WINE)))
    if not wine.is_file():
        raise RuntimeError('CrossOver CLI is unavailable; native Windows execution is not implied')
    return [str(wine), '--bottle', bottle, '--no-convert',
            '--workdir', windows_path(directory), '--cx-app', windows_path(executable),
            *[windows_path(value) if value.startswith('/') else value for value in arguments]]
