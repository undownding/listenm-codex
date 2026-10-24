"""Shared FAT image preparation for Lisem acceptance tests."""
import importlib.util
from pathlib import Path
import subprocess
from pyfatfs.PyFatFS import PyFatFS
ROOT = Path(__file__).resolve().parents[1]

def create_card(work, host='192.168.1.100', port='8787', token='tf-test-token'):
    spec=importlib.util.spec_from_file_location('prepare_tf', ROOT/'scripts/prepare-tf.py')
    module=importlib.util.module_from_spec(spec); spec.loader.exec_module(module)
    directory=work/'tf'; module.prepare(directory, ROOT/'.deps/arcs_mini')
    config=(directory/'config.ini').read_text().replace('192.168.1.100',host).replace('bridge_port=8787',f'bridge_port={port}').replace('replace-with-your-bridge-token',token)
    (directory/'config.ini').write_text(config)
    image=work/'tf.img'
    with image.open('wb') as stream: stream.truncate(128*1024*1024)
    subprocess.run(['mkfs.fat','-F','16',str(image)],check=True,stdout=subprocess.DEVNULL)
    fs=PyFatFS(str(image))
    try:
        for path in sorted(directory.rglob('*')):
            name=path.relative_to(directory).as_posix()
            if path.is_dir(): fs.makedir(name)
            else: fs.writebytes(name,path.read_bytes())
    finally: fs.close()
    return image
