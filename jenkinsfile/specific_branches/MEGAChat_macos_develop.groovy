pipeline {
    agent { label 'osx && arm64' }
    options {
        buildDiscarder(logRotator(numToKeepStr: '135', daysToKeepStr: '21'))
        gitLabConnection('GitLabConnectionJenkins')
        ansiColor('xterm')
    }
    environment {
        PATH = "/usr/local/bin:${env.PATH}"
        VCPKGPATH = "${env.HOME}/jenkins/vcpkg"
        MEGAQTPATH= "${env.HOME}/Qt-build/5.15.13/5.15.13/arm64"
        BUILD_TYPE= "Release"
        APIURL_TO_TEST = "https://g.api.mega.co.nz/"
        SDK_BRANCH = "develop"
        MEGACHAT_BRANCH = "develop"
        BUILD_OPTIONS = ' '
        BUILD_DIR = "build_dir"
        BUILD_DIR_X64 = "build_dir_x64"
    }
    stages {
        stage('Checkout SDK and MEGAchat'){
            steps {
                checkout([
                    $class: 'GitSCM', 
                    branches: [[name: "origin/${env.MEGACHAT_BRANCH}"]],
                    userRemoteConfigs: [[ url: "${env.GIT_URL_MEGACHAT}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                ])
                dir('third-party/mega'){
                    sh "echo Cloning SDK branch \"${SDK_BRANCH}\""
                    checkout([
                        $class: 'GitSCM',
                        branches: [[name: "origin/${SDK_BRANCH}"]],
                        userRemoteConfigs: [[ url: "${env.GIT_URL_SDK}", credentialsId: "12492eb8-0278-4402-98f0-4412abfb65c1" ]],
                    ])
                }
                script{
                    megachat_sources_workspace = WORKSPACE
                }
            }
        }
        stage('Build MEGAchat'){
            steps{
                dir(megachat_sources_workspace){
                    //Build for arm64
                    sh "echo Building for arm64 in ${BUILD_TYPE} mode"
                    sh "mkdir ${BUILD_DIR}"
                    sh "cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DVCPKG_ROOT=${VCPKGPATH} ${BUILD_OPTIONS} -DCMAKE_VERBOSE_MAKEFILE=ON \
                        -S ${megachat_sources_workspace} -B ${megachat_sources_workspace}/${BUILD_DIR} -DCMAKE_PREFIX_PATH=${MEGAQTPATH} \
                        -DCMAKE_OSX_ARCHITECTURES=arm64 -DENABLE_CHATLIB_QTAPP=OFF -DENABLE_CHATLIB_TESTS=ON -DUSE_FFMPEG=OFF -DUSE_FREEIMAGE=OFF" 
                    sh "cmake --build ${megachat_sources_workspace}/${BUILD_DIR} -j3"
                    
                    //build for x64
                    sh "echo Building for x64-crosscompiling in ${BUILD_TYPE} mode"
                    sh "mkdir ${BUILD_DIR_X64}"
                    sh "cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DVCPKG_ROOT=${VCPKGPATH} ${BUILD_OPTIONS} -DCMAKE_VERBOSE_MAKEFILE=ON \
                        -DCMAKE_OSX_ARCHITECTURES=x86_64 -DENABLE_CHATLIB_QTAPP=OFF -DENABLE_CHATLIB_TESTS=ON -DUSE_FFMPEG=OFF -DUSE_FREEIMAGE=OFF \
                        -S ${megachat_sources_workspace} -B ${megachat_sources_workspace}/${BUILD_DIR_X64} -DCMAKE_PREFIX_PATH=${MEGAQTPATH}"
                    sh "cmake --build ${megachat_sources_workspace}/${BUILD_DIR_X64} -j3"
                }
            }
        }
    }
    post {
        always {
            deleteDir()
        }
    }
}
// vim: syntax=groovy tabstop=4 shiftwidth=4
