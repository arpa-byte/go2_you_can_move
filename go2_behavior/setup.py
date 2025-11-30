from setuptools import find_packages, setup
import os
from glob import glob

package_name = 'go2_behavior'

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
        (os.path.join('share', package_name, 'config'), glob(os.path.join('config', '*.yaml'))),
        (os.path.join('share', package_name, 'rviz'), glob(os.path.join('rviz', '*.rviz'))),


    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='arpan',
    maintainer_email='arpan@todo.todo',
    description='A simple autonomous behavior package for Go2.',
    license='TODO: License declaration',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
            'sequence_controller = go2_behavior.sequence_controller:main',
            'coordinate_navigator = go2_behavior.coordinate_navigator:main',

        ],
    },
)