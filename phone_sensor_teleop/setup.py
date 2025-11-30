from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'phone_sensor_teleop'

setup(
    name=package_name,
    version='0.0.1',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # Include all launch files from the 'launch' directory
        (os.path.join('share', package_name, 'launch'), glob(os.path.join('launch', '*launch.[pxy][yma]*'))),
        # --- NEW: Include all yaml files from the 'config' directory ---
        (os.path.join('share', package_name, 'config'), glob(os.path.join('config', '*.yaml'))),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='arpan',
    maintainer_email='arpan@todo.todo',
    description='Teleoperation node for Go2 robot using phone sensor data.',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'phone_sensor_node = phone_sensor_teleop.phone_sensor_node:main',
        ],
    },
)